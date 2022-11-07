// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_controller.h"

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace hotspot_config {
namespace mojom = ::chromeos::hotspot_config::mojom;
}  // namespace hotspot_config

HotspotController::HotspotControlRequest::HotspotControlRequest(
    bool enabled,
    HotspotControlCallback callback)
    : enabled(enabled), callback(std::move(callback)) {}

HotspotController::HotspotControlRequest::~HotspotControlRequest() = default;

HotspotController::HotspotController() = default;

HotspotController::~HotspotController() = default;

void HotspotController::Init(HotspotStateHandler* hotspot_state_handler) {
  hotspot_state_handler_ = hotspot_state_handler;
}

void HotspotController::EnableHotspot(HotspotControlCallback callback) {
  queued_requests_.push(std::make_unique<HotspotControlRequest>(
      /*enabled=*/true, std::move(callback)));
  ProcessRequestQueue();
}

void HotspotController::DisableHotspot(HotspotControlCallback callback) {
  queued_requests_.push(std::make_unique<HotspotControlRequest>(
      /*enabled=*/false, std::move(callback)));
  ProcessRequestQueue();
}

void HotspotController::ProcessRequestQueue() {
  if (queued_requests_.empty())
    return;

  // A current request is already underway; wait until it has completed before
  // starting a new request.
  if (current_request_)
    return;

  current_request_ = std::move(queued_requests_.front());
  queued_requests_.pop();

  // Need to check the capabilities and do a final round of check tethering
  // readiness before enabling hotspot.
  if (current_request_->enabled) {
    CheckTetheringReadiness();
    return;
  }

  PerformSetTetheringEnabled(/*enabled=*/false);
}

void HotspotController::CheckTetheringReadiness() {
  if (hotspot_state_handler_->GetHotspotCapabilities().allow_status !=
      hotspot_config::mojom::HotspotAllowStatus::kAllowed) {
    CompleteCurrentRequest(
        hotspot_config::mojom::HotspotControlResult::kNotAllowed);
    return;
  }

  hotspot_state_handler_->CheckTetheringReadiness(
      base::BindOnce(&HotspotController::OnCheckTetheringReadiness,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnCheckTetheringReadiness(
    HotspotStateHandler::CheckTetheringReadinessResult result) {
  if (result != HotspotStateHandler::CheckTetheringReadinessResult::kReady) {
    CompleteCurrentRequest(
        hotspot_config::mojom::HotspotControlResult::kReadinessCheckFailed);
    return;
  }
  PerformSetTetheringEnabled(/*enabled=*/true);
}

void HotspotController::PerformSetTetheringEnabled(bool enabled) {
  ShillManagerClient::Get()->SetTetheringEnabled(
      enabled,
      base::BindOnce(&HotspotController::OnSetTetheringEnabledSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HotspotController::OnSetTetheringEnabledFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnSetTetheringEnabledSuccess() {
  CompleteCurrentRequest(hotspot_config::mojom::HotspotControlResult::kSuccess);
}

void HotspotController::OnSetTetheringEnabledFailure(
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Enable/disable tethering failed: " << error_name
                 << ", message: " << error_message;
  CompleteCurrentRequest(SetTetheringEnabledResultToMojom(error_name));
}

void HotspotController::CompleteCurrentRequest(
    hotspot_config::mojom::HotspotControlResult result) {
  std::move(current_request_->callback).Run(result);
  current_request_.reset();

  ProcessRequestQueue();
}

}  //  namespace ash