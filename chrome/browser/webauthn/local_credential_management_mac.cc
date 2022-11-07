// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/local_credential_management_mac.h"
#include "chrome/browser/webauthn/local_credential_management.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"
#include "device/fido/mac/credential_store.h"

LocalCredentialManagementMac::LocalCredentialManagementMac(
    device::fido::mac::AuthenticatorConfig config)
    : config_(config) {}

std::unique_ptr<LocalCredentialManagement> LocalCredentialManagement::Create(
    Profile* profile) {
  auto config =
      ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfigForProfile(
          profile);
  return std::make_unique<LocalCredentialManagementMac>(config);
}

void LocalCredentialManagementMac::HasCredentials(
    base::OnceCallback<void(bool)> callback) {
  if (!base::FeatureList::IsEnabled(features::kWebAuthConditionalUI)) {
    std::move(callback).Run(false);
    return;
  }
  Enumerate(
      base::BindOnce(
          [](absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
                 metadata) { return !metadata->empty(); })
          .Then(std::move(callback)));
}

void LocalCredentialManagementMac::Enumerate(
    base::OnceCallback<void(
        absl::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
        callback) {
  device::fido::mac::TouchIdCredentialStore credential_store(config_);
  absl::optional<std::list<device::fido::mac::Credential>> credentials =
      credential_store.FindResidentCredentials(/*rp_id=*/absl::nullopt);

  if (!credentials) {
    std::move(callback).Run(/*credentials=*/{});
    return;
  }
  std::vector<device::DiscoverableCredentialMetadata> credential_metadata;
  credential_metadata.reserve(credentials->size());
  for (auto& credential : *credentials) {
    credential_metadata.emplace_back(
        credential.rp_id, credential.credential_id,
        credential.metadata.ToPublicKeyCredentialUserEntity());
  }
  std::sort(credential_metadata.begin(), credential_metadata.end(),
            CredentialComparator());
  std::move(callback).Run(std::move(credential_metadata));
}

void LocalCredentialManagementMac::Delete(
    base::span<const uint8_t> credential_id,
    base::OnceCallback<void(bool)> callback) {
  device::fido::mac::TouchIdCredentialStore credential_store(config_);
  std::move(callback).Run(credential_store.DeleteCredentialById(credential_id));
}
