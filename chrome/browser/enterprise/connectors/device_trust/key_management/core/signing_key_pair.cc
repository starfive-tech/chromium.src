// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

#include "base/check.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

SigningKeyPair::SigningKeyPair(
    std::unique_ptr<crypto::UnexportableSigningKey> signing_key,
    KeyTrustLevel trust_level)
    : signing_key_(std::move(signing_key)), trust_level_(trust_level) {
  DCHECK(signing_key_);
}

SigningKeyPair::~SigningKeyPair() = default;

}  // namespace enterprise_connectors
