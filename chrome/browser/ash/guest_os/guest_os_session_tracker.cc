// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/containers/flat_tree.h"
#include "base/logging.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "content/public/browser/browser_thread.h"

namespace guest_os {

GuestInfo::GuestInfo(GuestId guest_id,
                     int64_t cid,
                     std::string username,
                     base::FilePath homedir,
                     std::string ipv4_address,
                     uint32_t sftp_vsock_port)
    : guest_id(std::move(guest_id)),
      cid(cid),
      username(std::move(username)),
      homedir(std::move(homedir)),
      ipv4_address(std::move(ipv4_address)),
      sftp_vsock_port(sftp_vsock_port) {}
GuestInfo::~GuestInfo() = default;
GuestInfo::GuestInfo(GuestInfo&&) = default;
GuestInfo::GuestInfo(const GuestInfo&) = default;
GuestInfo& GuestInfo::operator=(GuestInfo&&) = default;
GuestInfo& GuestInfo::operator=(const GuestInfo&) = default;

GuestOsSessionTracker* GuestOsSessionTracker::GetForProfile(Profile* profile) {
  return GuestOsSessionTrackerFactory::GetForProfile(profile);
}

GuestOsSessionTracker::GuestOsSessionTracker(std::string owner_id)
    : owner_id_(std::move(owner_id)) {
  if (!ash::ConciergeClient::Get() || !ash::CiceroneClient::Get()) {
    // These're null in unit tests unless explicitly set up. If missing, don't
    // register as an observer.
    return;
  }
  ash::ConciergeClient::Get()->AddVmObserver(this);
  ash::CiceroneClient::Get()->AddObserver(this);
  vm_tools::concierge::ListVmsRequest request;
  request.set_owner_id(owner_id_);
  ash::ConciergeClient::Get()->ListVms(
      request, base::BindOnce(&GuestOsSessionTracker::OnListVms,
                              weak_ptr_factory_.GetWeakPtr()));
}

GuestOsSessionTracker::~GuestOsSessionTracker() {
  if (!ash::ConciergeClient::Get() || !ash::CiceroneClient::Get()) {
    return;
  }
  ash::ConciergeClient::Get()->RemoveVmObserver(this);
  ash::CiceroneClient::Get()->RemoveObserver(this);
}

void GuestOsSessionTracker::OnListVms(
    absl::optional<vm_tools::concierge::ListVmsResponse> response) {
  if (!response) {
    LOG(ERROR)
        << "Failed to list VMs, assuming there aren't any already running";
    return;
  }
  for (const auto& vm : response->vms()) {
    vms_[vm.name()] = vm.vm_info();
  }
  vm_tools::cicerone::ListRunningContainersRequest req;
  req.set_owner_id(owner_id_);
  ash::CiceroneClient::Get()->ListRunningContainers(
      req, base::BindOnce(&GuestOsSessionTracker::OnListRunningContainers,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GuestOsSessionTracker::OnListRunningContainers(
    absl::optional<vm_tools::cicerone::ListRunningContainersResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "Failed to list containers, assuming there aren't any "
                  "already running";
    return;
  }
  for (const auto& container : response->containers()) {
    if (!vms_.contains(container.vm_name())) {
      continue;
    }
    vm_tools::cicerone::GetGarconSessionInfoRequest req;
    req.set_owner_id(owner_id_);
    req.set_vm_name(container.vm_name());
    req.set_container_name(container.container_name());
    ash::CiceroneClient::Get()->GetGarconSessionInfo(
        req, base::BindOnce(&GuestOsSessionTracker::OnGetGarconSessionInfo,
                            weak_ptr_factory_.GetWeakPtr(), container.vm_name(),
                            container.container_name()));
  }
}

void GuestOsSessionTracker::OnGetGarconSessionInfo(
    std::string vm_name,
    std::string container_name,
    absl::optional<vm_tools::cicerone::GetGarconSessionInfoResponse> response) {
  if (!response ||
      response->status() !=
          vm_tools::cicerone::GetGarconSessionInfoResponse::SUCCEEDED) {
    LOG(ERROR) << "Unable to get session info";
    return;
  }
  // Don't need ipv4 address yet so haven't plumbed it through. Once we get
  // around to port forwarding or similar we'll need it though.
  HandleNewGuest(vm_name, container_name, response->container_username(),
                 response->container_homedir(), "",
                 response->sftp_vsock_port());
}

// Returns information about a running guest. Returns nullopt if the guest
// isn't recognised e.g. it's not running.
absl::optional<GuestInfo> GuestOsSessionTracker::GetInfo(const GuestId& id) {
  auto iter = guests_.find(id);
  if (iter == guests_.end()) {
    return absl::nullopt;
  }
  return iter->second;
}

bool GuestOsSessionTracker::IsRunning(const GuestId& id) {
  return guests_.contains(id);
}

// ash::ConciergeClient::VmObserver overrides.
void GuestOsSessionTracker::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  vms_[signal.name()] = signal.vm_info();
}

void GuestOsSessionTracker::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  vms_.erase(signal.name());
  std::vector<GuestId> ids;
  for (const auto& pair : guests_) {
    if (pair.first.vm_name != signal.name()) {
      continue;
    }
    ids.push_back(pair.first);
  }
  for (const auto& id : ids) {
    guests_.erase(id);
    auto cb_list = container_shutdown_callbacks_.find(id);
    if (cb_list != container_shutdown_callbacks_.end()) {
      cb_list->second->Notify();
    }
  }
}

// ash::CiceroneClient::Observer overrides.
void GuestOsSessionTracker::OnContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  HandleNewGuest(signal.vm_name(), signal.container_name(),
                 signal.container_username(), signal.container_homedir(),
                 signal.ipv4_address(), signal.sftp_vsock_port());
}

void GuestOsSessionTracker::HandleNewGuest(const std::string& vm_name,
                                           const std::string& container_name,
                                           const std::string& username,
                                           const std::string& homedir,
                                           const std::string& ipv4_address,
                                           uint32_t sftp_vsock_port) {
  auto iter = vms_.find(vm_name);
  if (iter == vms_.end()) {
    LOG(ERROR)
        << "Received ContainerStarted signal for an unexpected VM, ignoring";
    return;
  }
  GuestId id{VmType::UNKNOWN, vm_name, container_name};
  GuestInfo info{id,           iter->second.cid(),
                 username,     base::FilePath(homedir),
                 ipv4_address, sftp_vsock_port};
  guests_.insert_or_assign(id, info);

  // If there're any pending container start callbacks for this guest run them.
  auto cb_list = container_start_callbacks_.find(id);
  if (cb_list != container_start_callbacks_.end()) {
    cb_list->second->Notify(info);
  }
}

void GuestOsSessionTracker::OnContainerShutdown(
    const vm_tools::cicerone::ContainerShutdownSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_) {
    return;
  }
  GuestId id{VmType::UNKNOWN, signal.vm_name(), signal.container_name()};
  guests_.erase(id);
  auto cb_list = container_shutdown_callbacks_.find(id);
  if (cb_list != container_shutdown_callbacks_.end()) {
    cb_list->second->Notify();
  }
}

base::CallbackListSubscription GuestOsSessionTracker::RunOnceContainerStarted(
    const GuestId& id,
    base::OnceCallback<void(GuestInfo)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto iter = guests_.find(id);
  if (iter != guests_.end()) {
    std::move(callback).Run(iter->second);
    return base::CallbackListSubscription();
  }
  auto& cb_list = container_start_callbacks_[id];
  if (!cb_list) {
    cb_list = std::make_unique<base::OnceCallbackList<void(GuestInfo)>>();
  }
  return cb_list->Add(std::move(callback));
}

void GuestOsSessionTracker::AddGuestForTesting(const GuestId& id,
                                               const GuestInfo& info) {
  guests_.insert_or_assign(id, info);
}

base::CallbackListSubscription GuestOsSessionTracker::RunOnShutdown(
    const GuestId& id,
    base::OnceCallback<void()> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto& cb_list = container_shutdown_callbacks_[id];
  if (!cb_list) {
    cb_list = std::make_unique<base::OnceCallbackList<void()>>();
  }
  return cb_list->Add(std::move(callback));
}

}  // namespace guest_os
