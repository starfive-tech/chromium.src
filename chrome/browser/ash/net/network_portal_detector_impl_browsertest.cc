// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_detector_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "components/account_id/account_id.h"
#include "components/captive_portal/core/captive_portal_testing_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class NetworkPortalWebDialog;

namespace {

const char* const kNotificationId =
    NetworkPortalNotificationController::kNotificationId;

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";
constexpr char kWifiServicePath[] = "/service/wifi";
constexpr char kWifiGuid[] = "wifi";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(FATAL) << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  ShillServiceClient::Get()->Connect(dbus::ObjectPath(service_path),
                                     base::DoNothing(),
                                     base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
}

void SetShillProxyAuthRequired() {
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(kWifiServicePath),
      shill::kPortalDetectionFailedStatusCodeProperty, base::Value(407),
      base::DoNothing(), base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
}

void SetState(const char* state) {
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(kWifiServicePath), shill::kStateProperty,
      base::Value(state), base::DoNothing(),
      base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class NetworkPortalDetectorImplBrowserTest
    : public LoginManagerTest,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  NetworkPortalDetectorImplBrowserTest()
      : LoginManagerTest(),
        test_account_id_(
            AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId)),
        network_portal_detector_(nullptr) {}

  NetworkPortalDetectorImplBrowserTest(
      const NetworkPortalDetectorImplBrowserTest&) = delete;
  NetworkPortalDetectorImplBrowserTest& operator=(
      const NetworkPortalDetectorImplBrowserTest&) = delete;

  ~NetworkPortalDetectorImplBrowserTest() override {}

  void TestPortalStateAndNotification(
      const char* shill_state,
      chromeos::NetworkState::PortalState portal_state,
      bool set_portal_status_for_proxy_auth,
      const std::u16string& expected_title,
      const std::u16string& expected_message,
      const std::u16string& expected_button_title,
      NetworkPortalDetector::CaptivePortalStatus portal_detector_status) {
    LoginUser(test_account_id_);
    content::RunAllPendingInMessageLoop();

    EXPECT_FALSE(display_service_->GetNotification(kNotificationId));

    // Set connected should not trigger portal detection.
    SetConnected(kWifiServicePath);

    chromeos::NetworkStateHandler* network_state_handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    const chromeos::NetworkState* default_network =
        network_state_handler->DefaultNetwork();
    ASSERT_TRUE(default_network);
    EXPECT_EQ(default_network->GetPortalState(),
              chromeos::NetworkState::PortalState::kOnline);
    EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
    EXPECT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE,
              network_portal_detector::GetInstance()->GetCaptivePortalStatus());

    // Setting a shill portal state should set portal detection and display a
    // notification
    if (set_portal_status_for_proxy_auth)
      SetShillProxyAuthRequired();
    SetState(shill_state);

    default_network = network_state_handler->DefaultNetwork();
    ASSERT_TRUE(default_network);
    EXPECT_EQ(default_network->GetPortalState(), portal_state);
    EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
    EXPECT_EQ(display_service_->GetNotification(kNotificationId)->title(),
              expected_title);
    EXPECT_EQ(display_service_->GetNotification(kNotificationId)->message(),
              expected_message);
    if (expected_button_title.empty()) {
      EXPECT_EQ(
          display_service_->GetNotification(kNotificationId)->buttons().size(),
          0);
    } else {
      EXPECT_EQ(display_service_->GetNotification(kNotificationId)
                    ->buttons()
                    .front()
                    .title,
                expected_button_title);
    }
    EXPECT_EQ(portal_detector_status,
              network_portal_detector::GetInstance()->GetCaptivePortalStatus());

    // Explicitly close the notification.
    display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                         kNotificationId, /*by_user=*/true);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService(kWifiServicePath, kWifiGuid, "wifi",
                             shill::kTypeWifi, shill::kStateIdle,
                             /*visible=*/true);

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/nullptr);

    network_portal_detector_ =
        new NetworkPortalDetectorImpl(test_loader_factory());
    // Takes ownership of |network_portal_detector_|:
    network_portal_detector::InitializeForTesting(network_portal_detector_);
    network_portal_detector_->enabled_ = true;
    set_detector(network_portal_detector_->captive_portal_detector_.get());
    network_portal_notification_controller_ =
        std::make_unique<NetworkPortalNotificationController>();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    network_portal_notification_controller_.reset();
  }

  void SetIgnoreNoNetworkForTesting() {
    network_portal_notification_controller_->SetIgnoreNoNetworkForTesting();
  }

 protected:
  AccountId test_account_id_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  NetworkPortalDetectorImpl* network_portal_detector_;
  std::unique_ptr<NetworkPortalNotificationController>
      network_portal_notification_controller_;
};

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       PRE_InSessionDetection) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       InSessionDetection) {
  TestPortalStateAndNotification(
      shill::kStateRedirectFound, chromeos::NetworkState::PortalState::kPortal,
      /*set_portal_status_for_proxy_auth=*/false,
      l10n_util::GetStringUTF16(IDS_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI),
      l10n_util::GetStringFUTF16(IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE_WIFI,
                                 u"wifi"),
      u"", NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
}

class NetworkPortalDetectorImplBrowserTestIgnoreProxy
    : public NetworkPortalDetectorImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NetworkPortalDetectorImplBrowserTestIgnoreProxy()
      : NetworkPortalDetectorImplBrowserTest() {}

  NetworkPortalDetectorImplBrowserTestIgnoreProxy(
      const NetworkPortalDetectorImplBrowserTestIgnoreProxy&) = delete;
  NetworkPortalDetectorImplBrowserTestIgnoreProxy& operator=(
      const NetworkPortalDetectorImplBrowserTestIgnoreProxy&) = delete;

 protected:
  void TestImpl(const bool preference_value);
};

void NetworkPortalDetectorImplBrowserTestIgnoreProxy::TestImpl(
    const bool preference_value) {
  LoginUser(test_account_id_);
  content::RunAllPendingInMessageLoop();

  SetIgnoreNoNetworkForTesting();

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kCaptivePortalAuthenticationIgnoresProxy, preference_value);

  // User connects to portalled wifi.
  SetConnected(kWifiServicePath);
  SetState(shill::kStateRedirectFound);

  // Check that the network is behind a portal and a notification is displayed.
  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
  EXPECT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            network_portal_detector::GetInstance()->GetCaptivePortalStatus());

  display_service_->GetNotification(kNotificationId)
      ->delegate()
      ->Click(absl::nullopt, absl::nullopt);

  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(preference_value,
            network_portal_notification_controller_->IsDialogShownForTesting());
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       PRE_TestWithPreference) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       TestWithPreference) {
  TestImpl(GetParam());
}

INSTANTIATE_TEST_SUITE_P(CaptivePortalAuthenticationIgnoresProxy,
                         NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                         testing::Bool());

class NetworkPortalDetectorImplBrowserTestUI2022Update
    : public NetworkPortalDetectorImplBrowserTest {
 public:
  NetworkPortalDetectorImplBrowserTestUI2022Update() {
    feature_list_.InitAndEnableFeature(features::kCaptivePortalUI2022);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTestUI2022Update,
                       InSessionDetectionRedirectFoundState) {
  TestPortalStateAndNotification(
      shill::kStateRedirectFound, chromeos::NetworkState::PortalState::kPortal,
      /*set_portal_status_for_proxy_auth=*/false,
      l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI),
      l10n_util::GetStringFUTF16(IDS_NEW_PORTAL_DETECTION_NOTIFICATION_MESSAGE,
                                 u"wifi"),
      l10n_util::GetStringUTF16(IDS_NEW_PORTAL_DETECTION_NOTIFICATION_BUTTON),
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
}

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTestUI2022Update,
                       InSessionDetectionPortalSuspectedState) {
  TestPortalStateAndNotification(
      shill::kStatePortalSuspected,
      chromeos::NetworkState::PortalState::kPortalSuspected,
      /*set_portal_status_for_proxy_auth=*/false,
      l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI),
      l10n_util::GetStringFUTF16(
          IDS_NEW_PORTAL_SUSPECTED_DETECTION_NOTIFICATION_MESSAGE, u"wifi"),
      l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_SUSPECTED_DETECTION_NOTIFICATION_BUTTON),
      // CaptivePortalStatus is online here because setting the
      // CaptivePortalResult from ChromeNetworkPortalDetector will default
      // to ONLINE from DetectionCompleted(). This results in the
      // NetworkStateHandler using the shill state.
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
}

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTestUI2022Update,
                       InSessionDetectionProxyAuthRequiredState) {
  TestPortalStateAndNotification(
      shill::kStatePortalSuspected,
      chromeos::NetworkState::PortalState::kProxyAuthRequired,
      /*set_portal_status_for_proxy_auth=*/true,
      l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI),
      l10n_util::GetStringFUTF16(
          IDS_NEW_PORTAL_PROXY_AUTH_REQUIRED_DETECTION_NOTIFICATION_MESSAGE,
          u"wifi"),
      l10n_util::GetStringUTF16(IDS_NEW_PORTAL_DETECTION_NOTIFICATION_BUTTON),
      // CaptivePortalStatus is online here because setting the
      // CaptivePortalResult from ChromeNetworkPortalDetector will default to
      // ONLINE from DetectionCompleted(). This results in the
      // NetworkStateHandler using the shill state.
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
}

}  // namespace ash
