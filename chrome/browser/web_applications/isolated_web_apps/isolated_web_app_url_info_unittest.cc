// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include <utility>

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::StartsWith;
using ::testing::Values;

constexpr char kValidIsolatedAppUrl[] =
    "isolated-app://"
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
    "?foo=bar#baz";
}  // namespace

using IsolatedWebAppUrlInfoTest = ::testing::Test;

TEST_F(IsolatedWebAppUrlInfoTest, CreateSucceedsWithValidUrl) {
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedAppUrl));

  EXPECT_THAT(url_info.has_value(), IsTrue());
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithInvalidScheme) {
  GURL gurl(
      "https://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");

  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(gurl);

  EXPECT_THAT(url_info.has_value(), IsFalse());
  EXPECT_THAT(url_info.error(), StartsWith("The URL scheme must be"));
}

TEST_F(IsolatedWebAppUrlInfoTest, CreateFailsWithInvalidUrl) {
  GURL gurl("aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");

  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(gurl);

  EXPECT_THAT(url_info.has_value(), IsFalse());
  EXPECT_THAT(url_info.error(), Eq("Invalid URL"));
}

TEST_F(IsolatedWebAppUrlInfoTest, ParseSignedWebBundleIdSucceedWithValidUrl) {
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedAppUrl));
  base::expected<web_package::SignedWebBundleId, std::string> bundle_id =
      url_info->ParseSignedWebBundleId();

  EXPECT_THAT(bundle_id.has_value(), IsTrue());
  EXPECT_THAT(bundle_id->type(),
              Eq(web_package::SignedWebBundleId::Type::kEd25519PublicKey));
  EXPECT_THAT(bundle_id->id(),
              Eq("aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"));
}

TEST_F(IsolatedWebAppUrlInfoTest, ParseSignedWebBundleIdFailsWithBadHostname) {
  GURL gurl(
      "isolated-app://"
      "ßerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");

  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(gurl);
  base::expected<web_package::SignedWebBundleId, std::string> bundle_id =
      url_info->ParseSignedWebBundleId();

  EXPECT_THAT(bundle_id.has_value(), IsFalse());
  EXPECT_THAT(bundle_id.error(),
              StartsWith("The host of isolated-app:// URLs must be a valid"));
}

TEST_F(IsolatedWebAppUrlInfoTest, OriginIsCorrect) {
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedAppUrl));

  EXPECT_THAT(url_info->origin().Serialize(),
              Eq("isolated-app://"
                 "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"));
}

TEST_F(IsolatedWebAppUrlInfoTest, AppIdIsHashedOrigin) {
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(GURL(kValidIsolatedAppUrl));

  EXPECT_THAT(url_info->app_id(), Eq("ckmbeioemjmabdoddhjadagkjknpeigi"));
}

class IsolatedWebAppGURLConversionTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(IsolatedWebAppGURLConversionTest, RemovesInvalidPartsFromUrls) {
  // GURL automatically removes port and credentials, and converts
  // `isolated-app:foo` to `isolated-app://foo`. This test is here to verify
  // that and therefore make sure that the `CHECK` inside
  // `ParseSignedWebBundleId` will never actually trigger as long as this test
  // succeeds.
  GURL gurl(GetParam().first);
  EXPECT_TRUE(gurl.IsStandard());
  EXPECT_EQ(gurl.spec(), GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppGURLConversionTest,
    Values(
        std::make_pair(kValidIsolatedAppUrl, kValidIsolatedAppUrl),
        // credentials
        std::make_pair(
            "isolated-app://"
            "foo:bar@aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
            "?foo=bar#baz",
            kValidIsolatedAppUrl),
        // explicit port
        std::make_pair(
            "isolated-app://"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic:123/"
            "?foo=bar#baz",
            kValidIsolatedAppUrl),
        // missing `//`
        std::make_pair(
            "isolated-app:"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
            "?foo=bar#baz",
            kValidIsolatedAppUrl)));

}  // namespace web_app
