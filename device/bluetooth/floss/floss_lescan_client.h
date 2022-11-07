// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_LESCAN_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_LESCAN_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace floss {

const char kScannerCallbackPath[] = "/org/chromium/bluetooth/scanner/callback";
const char kScannerCallbackInterfaceName[] =
    "org.chromium.bluetooth.ScannerCallback";

// TODO(b/217274013): Update structs to support filtering
class RSSISettings {};

class ScanSettings {};

class ScanFilter {};

struct ScanResult {
  std::string address;
  uint8_t addr_type;
  // TODO(b/217274013): add rest of fields
};

class ScannerClientObserver : public base::CheckedObserver {
 public:
  ScannerClientObserver() = default;
  ~ScannerClientObserver() override = default;

  // A scanner has been registered
  virtual void ScannerRegistered(device::BluetoothUUID uuid,
                                 uint8_t scanner_id,
                                 uint8_t status) {}

  // A scan result has been received
  virtual void ScanResultReceived(ScanResult scan_result) {}
};

// Low-level interface to Floss's LE Scan API.
class DEVICE_BLUETOOTH_EXPORT FlossLEScanClient : public FlossDBusClient,
                                                  public ScannerClientObserver {
 public:
  // Error: No such adapter.
  static const char kErrorUnknownAdapter[];

  // Creates the instance.
  static std::unique_ptr<FlossLEScanClient> Create();

  FlossLEScanClient(const FlossLEScanClient&) = delete;
  FlossLEScanClient& operator=(const FlossLEScanClient&) = delete;

  FlossLEScanClient();
  ~FlossLEScanClient() override;

  // Manage observers.
  void AddObserver(ScannerClientObserver* observer);
  void RemoveObserver(ScannerClientObserver* observer);

  // Initialize the LE Scan client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index) override;

  virtual void RegisterScanner(
      ResponseCallback<device::BluetoothUUID> callback);
  virtual void UnregisterScanner(ResponseCallback<bool> callback,
                                 uint8_t scanner_id);
  virtual void StartScan(ResponseCallback<Void> callback,
                         uint8_t scanner_id,
                         const ScanSettings& scan_settings,
                         const std::vector<ScanFilter>& filters);
  virtual void StopScan(ResponseCallback<Void> callback, uint8_t scanner_id);

 protected:
  // ScannerClientObserver overrides
  void ScannerRegistered(device::BluetoothUUID uuid,
                         uint8_t scanner_id,
                         uint8_t status) override;
  void ScanResultReceived(ScanResult scan_result) override;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Adapter managed by this client.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<ScannerClientObserver> observers_;

  // Service which implements the adapter interface.
  std::string service_name_;

 private:
  absl::optional<uint32_t> le_scan_callback_id_;

  ExportedCallbackManager<ScannerClientObserver>
      exported_scanner_callback_manager_{kScannerCallbackInterfaceName};

  void RegisterScannerCallback();

  void OnRegisterScannerCallback(DBusResult<uint32_t> ret);

  void OnUnregisterScannerCallback(DBusResult<bool> ret);

  template <typename R, typename... Args>
  void CallLEScanMethod(ResponseCallback<R> callback,
                        const char* member,
                        Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kGattInterface,
               object_path_, member, args...);
  }

  base::WeakPtrFactory<FlossLEScanClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_LESCAN_CLIENT_H_
