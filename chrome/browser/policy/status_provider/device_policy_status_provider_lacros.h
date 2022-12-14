// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_POLICY_STATUS_PROVIDER_LACROS_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_POLICY_STATUS_PROVIDER_LACROS_H_

#include "base/values.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"

// A policy status provider for device policy for Lacros.
class DevicePolicyStatusProviderLacros : public policy::PolicyStatusProvider {
 public:
  DevicePolicyStatusProviderLacros();
  ~DevicePolicyStatusProviderLacros() override;

  void SetDevicePolicyStatus(base::Value::Dict status);

  // PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

 private:
  base::Value::Dict device_policy_status_;
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_POLICY_STATUS_PROVIDER_LACROS_H_
