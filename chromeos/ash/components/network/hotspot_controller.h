// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-forward.h"

namespace ash {

// Handles enable or disable hotspot.
//
// Enabling the hotspot involves the following operations:
// 1. Check hotspot capabilities
// 2. Check tethering readiness
// 3. Enable tethering from Shill
//
// Enable or disable requests are queued and executes one request at a time in
// order.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotController {
 public:
  HotspotController();
  HotspotController(const HotspotController&) = delete;
  HotspotController& operator=(const HotspotController&) = delete;
  ~HotspotController();

  void Init(HotspotStateHandler* hotspot_state_handler);
  // Return callback for the EnableHotspot or DisableHotspot method.
  using HotspotControlCallback = base::OnceCallback<void(
      chromeos::hotspot_config::mojom::HotspotControlResult control_result)>;
  // Push the enable or disable hotspot request to the request queue and try to
  // execute. If another request is already being processed, the current request
  // will wait until the previous one is completed.
  void EnableHotspot(HotspotControlCallback callback);
  void DisableHotspot(HotspotControlCallback callback);

 private:
  // Represents hotspot enable or disable control request parameters. Requests
  // are queued and processed one at a time.
  struct HotspotControlRequest {
    HotspotControlRequest(bool enabled, HotspotControlCallback callback);
    HotspotControlRequest(const HotspotControlRequest&) = delete;
    HotspotControlRequest& operator=(const HotspotControlRequest&) = delete;
    ~HotspotControlRequest();

    bool enabled;
    HotspotControlCallback callback;
  };

  void ProcessRequestQueue();
  void CheckTetheringReadiness();
  void OnCheckTetheringReadiness(
      HotspotStateHandler::CheckTetheringReadinessResult result);
  void PerformSetTetheringEnabled(bool enabled);
  void OnSetTetheringEnabledSuccess();
  void OnSetTetheringEnabledFailure(const std::string& error_name,
                                    const std::string& error_message);
  void CompleteCurrentRequest(
      chromeos::hotspot_config::mojom::HotspotControlResult result);

  std::unique_ptr<HotspotControlRequest> current_request_;
  base::queue<std::unique_ptr<HotspotControlRequest>> queued_requests_;
  HotspotStateHandler* hotspot_state_handler_ = nullptr;
  base::WeakPtrFactory<HotspotController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_
