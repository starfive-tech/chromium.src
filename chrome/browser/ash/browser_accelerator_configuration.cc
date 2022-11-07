// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_accelerator_configuration.h"

#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"

namespace ash {

BrowserAcceleratorConfiguration::BrowserAcceleratorConfiguration()
    : AcceleratorConfiguration(ash::mojom::AcceleratorSource::kBrowser) {}

BrowserAcceleratorConfiguration::~BrowserAcceleratorConfiguration() = default;

const std::vector<AcceleratorInfo>&
BrowserAcceleratorConfiguration::GetConfigForAction(
    AcceleratorActionId action_id) {
  // TODO(jimmyxgong): Implement stub.
  return accelerator_infos_;
}

bool BrowserAcceleratorConfiguration::IsMutable() const {
  return false;
}

AcceleratorConfigResult BrowserAcceleratorConfiguration::AddUserAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  // TODO(jimmyxgong): Implement stub.
  return AcceleratorConfigResult::kSuccess;
}

AcceleratorConfigResult BrowserAcceleratorConfiguration::RemoveAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& accelerator) {
  // TODO(jimmyxgong): Implement stub.
  return AcceleratorConfigResult::kSuccess;
}

AcceleratorConfigResult BrowserAcceleratorConfiguration::ReplaceAccelerator(
    AcceleratorActionId action_id,
    const ui::Accelerator& old_acc,
    const ui::Accelerator& new_acc) {
  // TODO(jimmyxgong): Implement stub.
  return AcceleratorConfigResult::kSuccess;
}

AcceleratorConfigResult BrowserAcceleratorConfiguration::RestoreDefault(
    AcceleratorActionId action_id) {
  // TODO(jimmyxgong): Implement stub.
  return AcceleratorConfigResult::kSuccess;
}

AcceleratorConfigResult BrowserAcceleratorConfiguration::RestoreAllDefaults() {
  // TODO(jimmyxgong): Implement stub.
  return AcceleratorConfigResult::kSuccess;
}

}  // namespace ash
