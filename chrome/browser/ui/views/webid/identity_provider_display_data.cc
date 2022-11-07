// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/identity_provider_display_data.h"

IdentityProviderDisplayData::IdentityProviderDisplayData(
    const std::u16string& idp_etld_plus_one,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    const std::vector<content::IdentityRequestAccount>& accounts)
    : idp_etld_plus_one_(idp_etld_plus_one),
      idp_metadata_(idp_metadata),
      client_data_(client_data),
      accounts_(accounts) {}

IdentityProviderDisplayData::IdentityProviderDisplayData(
    const IdentityProviderDisplayData& other) = default;

IdentityProviderDisplayData::~IdentityProviderDisplayData() = default;
