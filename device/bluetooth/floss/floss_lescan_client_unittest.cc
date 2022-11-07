// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_lescan_client.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {
namespace {

using testing::_;

// Matches a dbus::MethodCall based on the method name (member).
MATCHER_P(HasMemberOf, member, "") {
  return arg->GetMember() == member;
}

const char kTestSender[] = ":0.1";
const int kTestSerial = 1;
constexpr uint8_t kTestUuidByteArray[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                          8, 9, 10, 11, 12, 13, 14, 15};
constexpr char kTestUuidStr[] = "00010203-0405-0607-0809-0a0b0c0d0e0f";
const uint8_t kTestScannerId = 10;
const uint8_t kTestStatus = 1;
const uint32_t kTestCallbackId = 1000;
constexpr char kTestDeviceAddr[] = "11:22:33:44:55:66";
const uint8_t kTestAddrType = 2;

}  // namespace

class FlossLEScanClientTest : public testing::Test,
                              public ScannerClientObserver {
 public:
  FlossLEScanClientTest() = default;

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    client_ = FlossLEScanClient::Create();
    client_->AddObserver(this);

    gatt_path_ = FlossDBusClient::GenerateGattPath(adapter_index_);
    callback_path_ = dbus::ObjectPath(kScannerCallbackPath);

    object_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kAdapterInterface, gatt_path_);

    EXPECT_CALL(*bus_.get(), GetObjectProxy(kAdapterInterface, gatt_path_))
        .WillRepeatedly(::testing::Return(object_proxy_.get()));
  }

  void TearDown() override {
    // Clean up the client first so it gets rid of all its references to the
    // various buses, object proxies, etc.
    client_.reset();
  }

  // ScannerClientObserver overrides
  void ScannerRegistered(device::BluetoothUUID uuid,
                         uint8_t scanner_id,
                         uint8_t status) override {
    fake_scanner_registered_info_ = {uuid, scanner_id, status};
  }

  void ScanResultReceived(ScanResult scan_result) override {
    fake_scan_result_ = scan_result;
  }

  void WriteScanResult(dbus::MessageWriter* writer, ScanResult* scan_result) {
    dbus::MessageWriter array(nullptr);

    writer->OpenArray("{sv}", &array);

    FlossDBusClient::WriteDictEntry(&array, "address", scan_result->address);
    FlossDBusClient::WriteDictEntry(&array, "addr_type",
                                    scan_result->addr_type);

    writer->CloseContainer(&array);
  }

  void TestOnScannerRegistered(
      dbus::ExportedObject::MethodCallCallback method_handler) {
    dbus::MethodCall method_call("some.interface",
                                 adapter::kOnScannerRegistered);
    method_call.SetPath(callback_path_);
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(kTestUuidByteArray, sizeof(kTestUuidByteArray));
    writer.AppendByte(kTestScannerId);
    writer.AppendByte(kTestStatus);

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ("", saved_response->GetErrorName());

    EXPECT_EQ(std::make_tuple(device::BluetoothUUID(kTestUuidStr),
                              kTestScannerId, kTestStatus),
              fake_scanner_registered_info_);
  }

  void TestOnScanResult(
      dbus::ExportedObject::MethodCallCallback method_handler) {
    dbus::MethodCall method_call("some.interface", adapter::kOnScanResult);
    method_call.SetPath(callback_path_);
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    ScanResult scan_result =
        ScanResult{.address = kTestDeviceAddr, .addr_type = kTestAddrType};
    WriteScanResult(&writer, &scan_result);

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ("", saved_response->GetErrorName());

    EXPECT_EQ(fake_scan_result_.address, kTestDeviceAddr);
    EXPECT_EQ(fake_scan_result_.addr_type, kTestAddrType);
  }

  int adapter_index_ = 5;
  dbus::ObjectPath gatt_path_;
  dbus::ObjectPath callback_path_;

  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockObjectProxy> object_proxy_;
  std::unique_ptr<FlossLEScanClient> client_;

  // For observer test inspections.
  absl::optional<std::tuple<device::BluetoothUUID, uint8_t, uint8_t>>
      fake_scanner_registered_info_;
  ScanResult fake_scan_result_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossLEScanClientTest> weak_ptr_factory_{this};
};

TEST_F(FlossLEScanClientTest, TestInitExportRegisterScanner) {
  scoped_refptr<::dbus::MockExportedObject> exported_callback =
      base::MakeRefCounted<::dbus::MockExportedObject>(bus_.get(),
                                                       callback_path_);

  // Expected exported callbacks
  dbus::ExportedObject::MethodCallCallback method_handler_on_scanner_registered;
  EXPECT_CALL(
      *exported_callback.get(),
      ExportMethod(kScannerCallbackInterfaceName, adapter::kOnScannerRegistered,
                   testing::_, testing::_))
      .WillOnce(testing::SaveArg<2>(&method_handler_on_scanner_registered));

  dbus::ExportedObject::MethodCallCallback method_handler_on_scan_result;
  EXPECT_CALL(*exported_callback.get(),
              ExportMethod(kScannerCallbackInterfaceName,
                           adapter::kOnScanResult, testing::_, testing::_))
      .WillOnce(testing::SaveArg<2>(&method_handler_on_scan_result));

  EXPECT_CALL(*bus_.get(), GetExportedObject(callback_path_))
      .WillRepeatedly(testing::Return(exported_callback.get()));

  // Expected call to RegisterScannerCallback when client is initialized
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kRegisterScannerCallback), _, _))
      .WillOnce([this](::dbus::MethodCall* method_call, int timeout_ms,
                       ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        dbus::ObjectPath param1;
        ASSERT_TRUE(msg.PopObjectPath(&param1));
        EXPECT_EQ(param1, this->callback_path_);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with uint32_t return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(kTestCallbackId);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });

  client_->Init(bus_.get(), kAdapterInterface, adapter_index_);

  // Test exported callbacks are correctly parsed
  ASSERT_TRUE(!!method_handler_on_scanner_registered);
  ASSERT_TRUE(!!method_handler_on_scan_result);

  TestOnScannerRegistered(method_handler_on_scanner_registered);
  TestOnScanResult(method_handler_on_scan_result);

  // Test RegisterScanner
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kRegisterScanner), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint32_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kTestCallbackId, param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with UUID return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendArrayOfBytes(kTestUuidByteArray,
                                  sizeof(kTestUuidByteArray));
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->RegisterScanner(
      base::BindLambdaForTesting([](DBusResult<device::BluetoothUUID> ret) {
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(device::BluetoothUUID(kTestUuidStr), *ret);
      }));

  // Test UnregisterScanner
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kUnregisterScanner), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint8_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kTestScannerId, param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with bool return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->UnregisterScanner(
      base::BindLambdaForTesting([](DBusResult<bool> ret) {
        // Check that there is no error and return is parsed correctly
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(true, *ret);
      }),
      kTestScannerId);

  // Expected UnregisterScannerCallback once client is cleaned up
  EXPECT_CALL(*object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kUnregisterScannerCallback), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint32_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kTestCallbackId, param1);
        EXPECT_FALSE(msg.HasMoreData());
      });
}

TEST_F(FlossLEScanClientTest, TestStartStopScan) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_);

  // Method of 3 parameters with no return.
  EXPECT_CALL(*object_proxy_.get(), DoCallMethodWithErrorResponse(
                                        HasMemberOf(adapter::kStartScan), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 3 parameters.
        // TODO(b/217274013): ScanSettings and ScanFilter currently being
        // ignored
        uint8_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadDBusParam(&msg, &param1));
        EXPECT_EQ(kTestScannerId, param1);
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->StartScan(base::BindLambdaForTesting([](DBusResult<Void> ret) {
                       // Check that there is no error
                       EXPECT_TRUE(ret.has_value());
                     }),
                     kTestScannerId, ScanSettings{}, std::vector<ScanFilter>());

  // Method of 1 parameter with no return.
  EXPECT_CALL(*object_proxy_.get(), DoCallMethodWithErrorResponse(
                                        HasMemberOf(adapter::kStopScan), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint8_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kTestScannerId, param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  client_->StopScan(base::BindLambdaForTesting([](DBusResult<Void> ret) {
                      // Check that there is no error
                      EXPECT_TRUE(ret.has_value());
                    }),
                    kTestScannerId);
}

}  // namespace floss
