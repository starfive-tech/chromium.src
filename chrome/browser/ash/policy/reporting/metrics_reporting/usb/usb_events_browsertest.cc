// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include "ash/components/settings/cros_settings_names.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Browser test that validate Usb added/removed events and telemetry collection
// when the`ReportDevicePeripherals policy is set/unset. These tests cases only
// cover USB added events and telemetry collection since FakeCrosHealthd doesn't
// expose a EmitUsbRemovedEventForTesting function.
constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";

class UsbEventsBrowserTest : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  UsbEventsBrowserTest() {
    // Add unaffiliated user for testing purposes.
    login_manager_mixin_.AppendRegularUsers(1);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kLoginManager);
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Set up affiliation for the test user.
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();

    device_policy_update->policy_data()->add_device_affiliation_ids(
        kTestAffiliationId);
    user_policy_update->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
  }

  void EnableUsbPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDevicePeripherals, true);
  }

  void DisableUsbPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDevicePeripherals, false);
  }

  bool NoUsbEventsEnqueued(const std::vector<::reporting::Record>& records) {
    return std::none_of(
        records.begin(), records.end(), [](::reporting::Record r) {
          return r.destination() == ::reporting::Destination::PERIPHERAL_EVENTS;
        });
  }

  void LoginAffiliatedUser() {
    const LoginManagerMixin::TestUserInfo user_info(test_account_id_);
    const auto& context =
        LoginManagerMixin::CreateDefaultUserContext(user_info);
    login_manager_mixin_.LoginAsNewRegularUser(context);
    test::WaitForPrimaryUserSessionStart();
  }

  void LoginUnaffiliatedUser() {
    login_manager_mixin_.LoginAsNewRegularUser();
    test::WaitForPrimaryUserSessionStart();
  }

  cros_healthd::mojom::TelemetryInfoPtr CreateUsbTelemetry() {
    constexpr uint8_t kClassId = 255;
    constexpr uint8_t kSubclassId = 1;
    constexpr uint16_t kVendorId = 65535;
    constexpr uint16_t kProductId = 1;
    constexpr char kVendorName[] = "VendorName";
    constexpr char kProductName[] = "ProductName";
    constexpr char kFirmwareVersion[] = "FirmwareVersion";

    cros_healthd::mojom::BusDevicePtr usb_device =
        cros_healthd::mojom::BusDevice::New();
    usb_device->vendor_name = kVendorName;
    usb_device->product_name = kProductName;
    usb_device->bus_info = cros_healthd::mojom::BusInfo::NewUsbBusInfo(
        cros_healthd::mojom::UsbBusInfo::New(
            kClassId, kSubclassId, /*protocol_id=*/0, kVendorId, kProductId,
            /*interfaces = */
            std::vector<cros_healthd::mojom::UsbBusInterfaceInfoPtr>(),
            cros_healthd::mojom::FwupdFirmwareVersionInfo::New(
                kFirmwareVersion,
                cros_healthd::mojom::FwupdVersionFormat::kPlain)));

    std::vector<cros_healthd::mojom::BusDevicePtr> usb_devices;
    usb_devices.push_back(std::move(usb_device));
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();
    telemetry_info->bus_result =
        cros_healthd::mojom::BusResult::NewBusDevices(std::move(usb_devices));
    return telemetry_info;
  }

  const AccountId test_account_id_ = AccountId::FromUserEmailGaiaId(
      kTestUserEmail,
      signin::GetTestGaiaIdForEmail(kTestUserEmail));

  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_account_id_};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{
      &mixin_host_, LoginManagerMixin::UserList(), &fake_gaia_mixin_};
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    UsbAddedEventCollectedWhenPolicyEnabledWithAffiliatedUser) {
  EnableUsbPolicy();

  LoginAffiliatedUser();

  MissiveClientTestObserver missive_observer_(
      ::reporting::Destination::PERIPHERAL_EVENTS);

  cros_healthd::FakeCrosHealthd::Get()->EmitUsbAddEventForTesting();
  std::tuple<::reporting::Priority, ::reporting::Record> entry =
      missive_observer_.GetNextEnqueuedRecord();
  ::reporting::Record record = std::get<1>(entry);
  ::reporting::MetricData record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));

  EXPECT_TRUE(record_data.has_telemetry_data());
  EXPECT_TRUE(record_data.telemetry_data().has_peripherals_telemetry());
  EXPECT_THAT(record_data.event_data().type(),
              ::testing::Eq(::reporting::MetricEventType::USB_ADDED));
  EXPECT_THAT(record.destination(),
              ::testing::Eq(reporting::Destination::PERIPHERAL_EVENTS));
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    UsbTelemetryCollectedWhenPolicyEnabledWithAffiliatedUser) {
  EnableUsbPolicy();

  MissiveClientTestObserver missive_observer_(
      ::reporting::Destination::PERIPHERAL_EVENTS);

  auto usb_telemetry = CreateUsbTelemetry();
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(usb_telemetry);

  // This triggers USB telemetry collection, a.k.a USB status updates
  LoginAffiliatedUser();

  std::tuple<::reporting::Priority, ::reporting::Record> entry =
      missive_observer_.GetNextEnqueuedRecord();
  ::reporting::Record record = std::get<1>(entry);
  ::reporting::MetricData record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));

  EXPECT_TRUE(record_data.has_telemetry_data());
  EXPECT_TRUE(record_data.telemetry_data().has_peripherals_telemetry());
  // Even though USB status updates are triggered by affiliated login, they're
  // technically telemetry, not events, so their event type is
  // EVENT_TYPE_UNSPECIFIED
  EXPECT_THAT(
      record_data.event_data().type(),
      ::testing::Eq(::reporting::MetricEventType::EVENT_TYPE_UNSPECIFIED));
  EXPECT_THAT(record.destination(),
              ::testing::Eq(::reporting::Destination::PERIPHERAL_EVENTS));
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    NoUsbEventsOrTelemetryWhenPolicyEnabledWithUnaffiliatedUser) {
  EnableUsbPolicy();

  MissiveClientTestObserver missive_observer_(
      ::reporting::Destination::PERIPHERAL_EVENTS);

  LoginUnaffiliatedUser();

  cros_healthd::FakeCrosHealthd::Get()->EmitUsbAddEventForTesting();
  EXPECT_TRUE(NoUsbEventsEnqueued(
      MissiveClient::Get()->GetTestInterface()->GetEnqueuedRecords(
          ::reporting::Priority::SECURITY)));
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    NoUsbEventsOrTelemetryWhenPolicyDisabledWithAffiliatedUser) {
  DisableUsbPolicy();

  LoginAffiliatedUser();

  cros_healthd::FakeCrosHealthd::Get()->EmitUsbAddEventForTesting();

  // Shouldn't be any USB event related records in the MissiveClient queue
  EXPECT_TRUE(NoUsbEventsEnqueued(
      MissiveClient::Get()->GetTestInterface()->GetEnqueuedRecords(
          ::reporting::Priority::SECURITY)));
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    NoUsbEventsOrTelemetryWhenPolicyDisabledWithUnaffiliatedUser) {
  DisableUsbPolicy();

  LoginUnaffiliatedUser();

  cros_healthd::FakeCrosHealthd::Get()->EmitUsbAddEventForTesting();

  // Shouldn't be any USB event related records in the MissiveClient queue
  EXPECT_TRUE(NoUsbEventsEnqueued(
      MissiveClient::Get()->GetTestInterface()->GetEnqueuedRecords(
          ::reporting::Priority::SECURITY)));
}

}  // namespace
}  // namespace chromeos
