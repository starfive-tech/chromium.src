// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_gatt_connection_floss.h"

#include "base/bind.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

namespace floss {

BluetoothGattConnectionFloss::BluetoothGattConnectionFloss(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const FlossDeviceId& device_id)
    : BluetoothGattConnection(adapter.get(), device_id.address) {
  DCHECK(adapter_.get());
  DCHECK(!device_id.address.empty());

  id_ = device_id;
  connected_ = true;
  floss::FlossDBusManager::Get()->GetAdapterClient()->AddObserver(this);
}

BluetoothGattConnectionFloss::~BluetoothGattConnectionFloss() {
  floss::FlossDBusManager::Get()->GetAdapterClient()->RemoveObserver(this);
  Disconnect();
}

bool BluetoothGattConnectionFloss::IsConnected() {
  return connected_;
}

void BluetoothGattConnectionFloss::Disconnect() {
  if (!connected_) {
    DVLOG(1) << "Connection already inactive";
    return;
  }

  connected_ = false;
  BluetoothGattConnection::Disconnect();
}

void BluetoothGattConnectionFloss::AdapterDeviceDisconnected(
    const FlossDeviceId& device) {
  if (device.address != id_.address)
    return;

  connected_ = false;
}

}  // namespace floss
