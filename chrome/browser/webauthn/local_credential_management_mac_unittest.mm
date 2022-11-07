// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "chrome/browser/webauthn/local_credential_management_mac.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/fake_keychain.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

static const device::PublicKeyCredentialUserEntity kUser({1, 2, 3},
                                                         "doe@example.com",
                                                         "John Doe");
constexpr char kRpId[] = "example.com";

class LocalCredentialManagementTest : public testing::Test {
 protected:
  LocalCredentialManagementTest() = default;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  device::fido::mac::AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  LocalCredentialManagementMac local_cred_man_{config_};
  device::fido::mac::ScopedFakeKeychain keychain_{
      config_.keychain_access_group};
  device::fido::mac::TouchIdCredentialStore store_{config_};
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebAuthConditionalUI};
};

class WebAuthConditionalUIFlagOffTest : public LocalCredentialManagementTest {
 protected:
  WebAuthConditionalUIFlagOffTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(features::kWebAuthConditionalUI);
  }
};

TEST_F(WebAuthConditionalUIFlagOffTest, FeatureFlagOff) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  EXPECT_TRUE(credential);
  local_cred_man_.HasCredentials(callback.callback());
  callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(callback.TakeResult()));
}

TEST_F(LocalCredentialManagementTest, NoCredentials) {
  device::test::TestCallbackReceiver<bool> callback;
  local_cred_man_.HasCredentials(callback.callback());
  callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(callback.TakeResult()));

  device::test::TestCallbackReceiver<
      absl::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_callback;
  local_cred_man_.Enumerate(enumerate_callback.callback());
  enumerate_callback.WaitForCallback();
  auto result = std::get<0>(enumerate_callback.TakeResult());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(LocalCredentialManagementTest, OneCredential) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  EXPECT_TRUE(credential);
  local_cred_man_.HasCredentials(callback.callback());
  callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(callback.TakeResult()));

  device::test::TestCallbackReceiver<
      absl::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_callback;
  local_cred_man_.Enumerate(enumerate_callback.callback());
  enumerate_callback.WaitForCallback();
  const absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = std::get<0>(enumerate_callback.TakeResult());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).rp_id, kRpId);
}

TEST_F(LocalCredentialManagementTest, DeleteCredential) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(credentials);
  EXPECT_EQ(credentials->size(), 1u);
  local_cred_man_.Delete(credential->first.credential_id, callback.callback());
  callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(callback.TakeResult()));
  credentials = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(credentials);
  EXPECT_EQ(store_.FindResidentCredentials(kRpId)->size(), 0u);

  local_cred_man_.Delete(std::vector<uint8_t>{8}, callback.callback());
  callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(callback.TakeResult()));
}

}  // namespace
