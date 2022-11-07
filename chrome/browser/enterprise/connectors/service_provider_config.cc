// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "base/json/json_reader.h"

#if defined(USE_OFFICIAL_ENTERPRISE_CONNECTORS_API_KEYS)
#include "google_apis/internal/enterprise_connectors_api_keys.h"
#endif

// Used to indicate an unset key/id/secret.  This works better with
// various unit tests than leaving the token empty.
#define DUMMY_API_TOKEN "dummytoken"

#if !defined(CLIENT_ID_CONNECTOR_PARTNER_BOX)
#define CLIENT_ID_CONNECTOR_PARTNER_BOX DUMMY_API_TOKEN
#endif

#if !defined(CLIENT_SECRET_CONNECTOR_PARTNER_BOX)
#define CLIENT_SECRET_CONNECTOR_PARTNER_BOX DUMMY_API_TOKEN
#endif

namespace enterprise_connectors {

namespace {

constexpr std::array<SupportedTag, 2> kGoogleDlpSupportedTags = {{
    {
        .name = "malware",
        .display_name = "Threat protection",
        .max_file_size = 52428800,
    },
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr AnalysisConfig kGoogleAnalysisConfig = {
    .url = "https://safebrowsing.google.com/safebrowsing/uploads/scan",
    .supported_tags = base::span<const SupportedTag>(kGoogleDlpSupportedTags),
};

constexpr std::array<SupportedTag, 1> kLocalTestSupportedTags = {{
    {
        .name = "dlp",
        .display_name = "Sensitive data protection",
        .max_file_size = 52428800,
    },
}};

constexpr AnalysisConfig kLocalTestUserAnalysisConfig = {
    .local_path = "path_user",
    .supported_tags = base::span<const SupportedTag>(kLocalTestSupportedTags),
    .user_specific = true,
};

constexpr AnalysisConfig kLocalTestSystemAnalysisConfig = {
    .local_path = "path_system",
    .supported_tags = base::span<const SupportedTag>(kLocalTestSupportedTags),
    .user_specific = false,
};

constexpr ReportingConfig kGoogleReportingConfig = {
    .url = "https://chromereporting-pa.googleapis.com/v1/events",
};

constexpr FileSystemConfig kBoxFileSystemConfig = {
    .home = "https://box.com",
    .authorization_endpoint = "https://account.box.com/api/oauth2/authorize",
    .token_endpoint = "https://api.box.com/oauth2/token",
    .max_direct_size = 20971520,
    .scopes = {},
    .disable = {"box.com", "boxcloud.com"},
    .client_id = CLIENT_ID_CONNECTOR_PARTNER_BOX,
    .client_secret = CLIENT_SECRET_CONNECTOR_PARTNER_BOX,
};

}  // namespace

const ServiceProviderConfig* GetServiceProviderConfig() {
  static constexpr ServiceProviderConfig kServiceProviderConfig =
      base::MakeFixedFlatMap<base::StringPiece, ServiceProvider>({
          {
              "google",
              {
                  .display_name = "Google Cloud",
                  .analysis = &kGoogleAnalysisConfig,
                  .reporting = &kGoogleReportingConfig,
              },
          },
          {
              "box",
              {
                  .display_name = "Box",
                  .file_system = &kBoxFileSystemConfig,
              },
          },
          // TODO(b/226560946): Add the actual local content analysis service
          // providers to this config.
          {
              "local_user_agent",
              {
                  .display_name = "Test user agent",
                  .analysis = &kLocalTestUserAnalysisConfig,
              },
          },
          {
              "local_system_agent",
              {
                  .display_name = "Test system agent",
                  .analysis = &kLocalTestSystemAnalysisConfig,
              },
          },
      });
  return &kServiceProviderConfig;
}

}  // namespace enterprise_connectors
