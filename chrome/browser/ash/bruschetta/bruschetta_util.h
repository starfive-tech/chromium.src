// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_

#include "chrome/browser/ash/guest_os/guest_id.h"

namespace bruschetta {

extern const char kBruschettaVmName[];
extern const char kBruschettaDisplayName[];

enum class BruschettaResult {
  kUnknown,
  kSuccess,
  kDlcInstallError,
  kBiosNotAccessible,
  kStartVmFailed,
  kTimeout,
};

// Returns the string name of the BruschettaResult.
const char* BruschettaResultString(const BruschettaResult res);

guest_os::GuestId GetBruschettaId();

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
