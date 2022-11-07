// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

absl::optional<std::string> IsolatedWebAppValidator::ValidateIntegrityBlock(
    web_package::SignedWebBundleId web_bundle_id,
    const std::vector<web_package::Ed25519PublicKey>& public_key_stack) {
  if (public_key_stack.empty()) {
    return "The Isolated Web App must have at least one signature.";
  }

  // TODO: Check whether we trust the public keys contained in the integrity
  // block here.
  return absl::nullopt;
}

absl::optional<std::string> IsolatedWebAppValidator::ValidateMetadata(
    web_package::SignedWebBundleId web_bundle_id,
    const GURL& primary_url,
    const std::vector<GURL>& entries) {
  // Verify that the primary URL of the bundle corresponds to the Signed Web
  // Bundle ID.
  GURL expected_primary_url(std::string(chrome::kIsolatedAppScheme) +
                            url::kStandardSchemeSeparator + web_bundle_id.id());
  DCHECK(expected_primary_url.is_valid());
  if (primary_url != expected_primary_url) {
    return base::StringPrintf(
        "Invalid metadata: Primary URL must be %s, but was %s",
        expected_primary_url.spec().c_str(), primary_url.spec().c_str());
  }

  // Verify that the bundle only contains isolated-app:// URLs using the
  // Signed Web Bundle ID as their host.
  for (const GURL& entry : entries) {
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
        IsolatedWebAppUrlInfo::Create(entry);
    if (!url_info.has_value()) {
      return base::StringPrintf(
          "Invalid metadata: The URL of an exchange is invalid: %s",
          url_info.error().c_str());
    }

    auto entry_web_bundle_id = url_info->ParseSignedWebBundleId();
    if (!entry_web_bundle_id.has_value()) {
      return base::StringPrintf(
          "Invalid metadata: The URL of an exchange is invalid: %s",
          entry_web_bundle_id.error().c_str());
    }
    if (entry_web_bundle_id != web_bundle_id) {
      return base::StringPrintf(
          "Invalid metadata: The URL of an exchange contains the wrong Signed "
          "Web Bundle ID: %s",
          entry_web_bundle_id->id().c_str());
    }
    if (entry.has_ref()) {
      return base::StringPrintf(
          "Invalid metadata: The URL of an exchange is invalid: URLs must not "
          "have a fragment part.");
    }
    if (entry.has_query()) {
      return base::StringPrintf(
          "Invalid metadata: The URL of an exchange is invalid: URLs must not "
          "have a query part.");
    }
  }

  return absl::nullopt;
}

}  // namespace web_app
