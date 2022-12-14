// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui_handler_impl.h"

#include "base/base64.h"
#include "base/bind.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_browsertest_base.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_callback.pb.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

using ParentAccessUIHandlerImplBrowserTest =
    ParentAccessChildUserBrowserTestBase;

// Verify that the access token is successfully fetched.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenSuccess) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));

  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
                  status);
      }));
}

// Verifies that access token fetch errors are recorded.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenError) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));

  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Trigger failure to issue access token.
  identity_test_env_->SetAutomaticIssueOfAccessTokens(false);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kError, status);
      }));
}

// Verifies that only one access token fetch is possible at a time.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenOnlyOneFetchAtATimeError) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
                  status);
      }));

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(
            parent_access_ui::mojom::GetOAuthTokenStatus::kOnlyOneFetchAtATime,
            status);
      }));
}

// Verifies that the ParentVerified status is handled correctly.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       ParentVerifiedParsed) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  kids::platform::parentaccess::client::proto::OnParentVerified*
      on_parent_verified = parent_access_callback.mutable_on_parent_verified();
  kids::platform::parentaccess::client::proto::ParentAccessToken* pat =
      on_parent_verified->mutable_parent_access_token();
  pat->set_token("TEST_TOKEN");
  kids::platform::parentaccess::client::proto::Timestamp* expire_time =
      pat->mutable_expire_time();
  expire_time->set_seconds(123456);
  expire_time->set_nanos(567890);

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify the Parent Verified callback is parsed.
            // TODO(b/241166361): Verify token value and expire time.
            EXPECT_EQ(parent_access_ui::mojom::ParentAccessServerMessageType::
                          kParentVerified,
                      message->type);
          }));
}

// Verifies that the ConsentDeclined status is ignored.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       ConsentDeclinedParsed) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_consent_declined();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
          }));
}

// Verifies that the OnPageSizeChanged status is ignored.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       OnPageSizeChangedIgnored) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_page_size_changed();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
          }));
}

// Verifies that the OnCommunicationEstablished status is ignored.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       OnCommunicationEstablishedIgnored) {
  // Open the Parent Access WebUI URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIParentAccessURL)));
  EXPECT_TRUE(content::WaitForLoadStop(contents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_communication_established();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
          }));
}

}  // namespace chromeos
