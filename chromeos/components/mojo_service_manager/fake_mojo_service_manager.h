// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_FAKE_MOJO_SERVICE_MANAGER_H_
#define CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_FAKE_MOJO_SERVICE_MANAGER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos::mojo_service_manager {

// Provides fake implementation of the service manager for testing.
//
// The behaviors are different from the real service:
// * No permission checking. A fake identity can be set when binding the mojo
//   remote. It will be used as the identity of owner or requester.
// * Register always succeeds, except that the services has already been
//   registered.
// * Request always succeeds. Timeout is ignored (always wait forever).
// * Query returns "not found" if the service is not yet registered / requested,
//   otherwise returns the state of the service.
// * The ServiceObserver can receive all the event (no permission checking).
//
class COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER) FakeMojoServiceManager
    : public mojom::ServiceManager {
 public:
  FakeMojoServiceManager();
  FakeMojoServiceManager(FakeMojoServiceManager&) = delete;
  FakeMojoServiceManager& operator=(FakeMojoServiceManager&) = delete;
  ~FakeMojoServiceManager() override;

  // Adds a new pipe and pass the pending remote. The identity of remote will
  // be bound to |security_context|.
  mojo::PendingRemote<mojom::ServiceManager> AddNewPipeAndPassRemote(
      const std::string& security_context);

 private:
  // Keeps all the objects related to a mojo service.
  struct ServiceState {
    ServiceState();
    ~ServiceState();

    // The pending requests to be sent after the service is available.
    std::vector<
        std::pair<mojom::ProcessIdentityPtr, mojo::ScopedMessagePipeHandle>>
        pending_requests;
    // The owner of the service.
    mojom::ProcessIdentityPtr owner;
    // The mojo remote to the service provider.
    mojo::Remote<mojom::ServiceProvider> service_provider;
  };

  // mojom::ServiceManager overrides.
  void Register(
      const std::string& service_name,
      mojo::PendingRemote<mojom::ServiceProvider> service_provider) override;
  void Request(const std::string& service_name,
               absl::optional<base::TimeDelta> timeout,
               mojo::ScopedMessagePipeHandle receiver) override;
  void Query(const std::string& service_name, QueryCallback callback) override;
  void AddServiceObserver(
      mojo::PendingRemote<mojom::ServiceObserver> observer) override;

  // Handles disconnection from service providers.
  void ServiceProviderDisconnectHandler(const std::string& service_name);

  // Sends service event to all the observers.
  void SendServiceEvent(mojom::ServiceEventPtr event);

  // The receiver set to provide the fake service manager.
  mojo::ReceiverSet<mojom::ServiceManager, mojom::ProcessIdentityPtr>
      receiver_set_;
  // The map of the service name to the service state.
  std::map<std::string, ServiceState> service_map_;
  // The remote set for the service observer.
  mojo::RemoteSet<mojom::ServiceObserver> service_observers_;
};

}  // namespace chromeos::mojo_service_manager

#endif  // CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_FAKE_MOJO_SERVICE_MANAGER_H_
