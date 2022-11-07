// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_PLATFORM_DELEGATE_H_

#include "components/device_signals/core/system_signals/posix/posix_platform_delegate.h"

namespace device_signals {

class MacPlatformDelegate : public PosixPlatformDelegate {
 public:
  MacPlatformDelegate();
  ~MacPlatformDelegate() override;

  // PlatformDelegate:
  bool ResolveFilePath(const base::FilePath& file_path,
                       base::FilePath* resolved_file_path) override;
  absl::optional<std::string> GetSigningCertificatePublicKeyHash(
      const base::FilePath& file_path) override;

  // Verifies if `file_path` points to an app bundle and then returns the
  // executable path for the file inside the bundle. If `file_path` does not
  // point to a bundle, it is returned as-is.
  base::FilePath GetBinaryFilePath(const base::FilePath& file_path);
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_MAC_MAC_PLATFORM_DELEGATE_H_
