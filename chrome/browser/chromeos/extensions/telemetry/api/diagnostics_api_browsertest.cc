// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_diagnostics_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/telemetry_extension/diagnostics_service_ash.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_diagnostics_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

class TelemetryExtensionDiagnosticsApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionDiagnosticsApiBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::DiagnosticsServiceAsh::Factory::SetForTesting(
        &fake_diagnostics_service_factory_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  ~TelemetryExtensionDiagnosticsApiBrowserTest() override = default;

  TelemetryExtensionDiagnosticsApiBrowserTest(
      const TelemetryExtensionDiagnosticsApiBrowserTest&) = delete;
  TelemetryExtensionDiagnosticsApiBrowserTest& operator=(
      const TelemetryExtensionDiagnosticsApiBrowserTest&) = delete;

 protected:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns whether the Diagnostics interface is available. It may
  // not be available on earlier versions of ash-chrome.
  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::DiagnosticsService>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  void SetServiceForTesting(
      std::unique_ptr<FakeDiagnosticsService> fake_diagnostics_service_impl) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_diagnostics_service_factory_.SetCreateInstanceResponse(
        std::move(fake_diagnostics_service_impl));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Replace the production DiagnosticsService with a fake for testing.
    mojo::Remote<crosapi::mojom::DiagnosticsService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::DiagnosticsService>();
    DCHECK(remote);
    remote.reset();
    fake_diagnostics_service_impl->BindPendingReceiver(
        remote.BindNewPipeAndPassReceiver());
    fake_diagnostics_service_impl_ = std::move(fake_diagnostics_service_impl);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  FakeDiagnosticsServiceFactory fake_diagnostics_service_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeDiagnosticsService> fake_diagnostics_service_impl_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       LacrosServiceNotAvailableError) {
  if (IsServiceAvailable()) {
    return;
  }

  std::string service_worker = R"(
    const tests = [
      // Diagnostics APIs.
      async function getAvailableRoutines() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.getAvailableRoutines(),
            'Error: API chrome.os.diagnostics.getAvailableRoutines failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function getRoutineUpdate() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.getRoutineUpdate(
              {
                id: 12345,
                command: 'status'
              }
            ),
            'Error: API chrome.os.diagnostics.getRoutineUpdate failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runAcPowerRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runAcPowerRoutine(
              {
                expected_status: 'connected',
                expected_power_type: 'ac_power'
              }
            ),
            'Error: API chrome.os.diagnostics.runAcPowerRoutine failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runBatteryCapacityRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryCapacityRoutine(),
            'Error: API chrome.os.diagnostics.runBatteryCapacityRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runBatteryChargeRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryChargeRoutine(
              {
                length_seconds: 1000,
                minimum_charge_percent_required: 1
              }
            ),
            'Error: API chrome.os.diagnostics.runBatteryChargeRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runBatteryDischargeRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryDischargeRoutine(
              {
                length_seconds: 10,
                maximum_discharge_percent_allowed: 15
              }
            ),
            'Error: API chrome.os.diagnostics.runBatteryDischargeRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runBatteryHealthRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runBatteryHealthRoutine(),
            'Error: API chrome.os.diagnostics.runBatteryHealthRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runCpuCacheRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuCacheRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: API chrome.os.diagnostics.runCpuCacheRoutine failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runCpuFloatingPointAccuracyRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuFloatingPointAccuracyRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: API chrome.os.diagnostics.' +
            'runCpuFloatingPointAccuracyRoutine failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runCpuPrimeSearchRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuPrimeSearchRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: API chrome.os.diagnostics.runCpuPrimeSearchRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runCpuStressRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runCpuStressRoutine(
              {
                length_seconds: 120
              }
            ),
            'Error: API chrome.os.diagnostics.runCpuStressRoutine failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runDiskReadRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runDiskReadRoutine(
              {
                type: 'random',
                length_seconds: 60,
                file_size_mb: 200
              }
            ),
            'Error: API chrome.os.diagnostics.runDiskReadRoutine failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runLanConnectivityRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runLanConnectivityRoutine(),
            'Error: API chrome.os.diagnostics.runLanConnectivityRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runMemoryRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runMemoryRoutine(),
            'Error: API chrome.os.diagnostics.runMemoryRoutine failed. ' +
            'Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runNvmeWearLevelRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runNvmeWearLevelRoutine(
              {
                wear_level_threshold: 80
              }
            ),
            'Error: API chrome.os.diagnostics.runNvmeWearLevelRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
      async function runSmartctlCheckRoutine() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.runSmartctlCheckRoutine(),
            'Error: API chrome.os.diagnostics.runSmartctlCheckRoutine ' +
            'failed. Not supported by ash browser'
        );
        chrome.test.succeed();
      },
    ];

    chrome.test.runTests([
      async function allAPIsTested() {
        getTestNames = function(arr) {
          return arr.map(item => item.name);
        }
        getMethods = function(obj) {
          return Object.getOwnPropertyNames(obj).filter(
            item => typeof obj[item] === 'function');
        }
        apiNames = [
          ...getMethods(chrome.os.diagnostics)
        ];
        chrome.test.assertEq(getTestNames(tests), apiNames);
        chrome.test.succeed();
      },
      ...tests
    ]);
  )";

  CreateExtensionAndRunServiceWorker(service_worker);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetAvailableRoutinesSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  {
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetAvailableRoutines({
        crosapi::mojom::DiagnosticsRoutineEnum::kAcPower,
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCapacity,
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCharge,
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryDischarge,
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryHealth,
        crosapi::mojom::DiagnosticsRoutineEnum::kCpuCache,
        crosapi::mojom::DiagnosticsRoutineEnum::kFloatingPointAccuracy,
        crosapi::mojom::DiagnosticsRoutineEnum::kPrimeSearch,
        crosapi::mojom::DiagnosticsRoutineEnum::kCpuStress,
        crosapi::mojom::DiagnosticsRoutineEnum::kDiskRead,
        crosapi::mojom::DiagnosticsRoutineEnum::kLanConnectivity,
        crosapi::mojom::DiagnosticsRoutineEnum::kMemory,
        crosapi::mojom::DiagnosticsRoutineEnum::kNvmeWearLevel,
        crosapi::mojom::DiagnosticsRoutineEnum::kSmartctlCheck,
    });

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getAvailableRoutines() {
        const response =
          await chrome.os.diagnostics.getAvailableRoutines();
        chrome.test.assertEq(
          {
            routines: [
              "ac_power",
              "battery_capacity",
              "battery_charge",
              "battery_discharge",
              "battery_health",
              "cpu_cache",
              "cpu_floating_point_accuracy",
              "cpu_prime_search",
              "cpu_stress",
              "disk_read",
              "lan_connectivity",
              "memory",
              "nvme_wear_level",
              "smartctl_check"
            ]
          }, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetRoutineUpdateNonInteractiveSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto nonInteractiveRoutineUpdate =
        crosapi::mojom::DiagnosticsNonInteractiveRoutineUpdate::New();
    nonInteractiveRoutineUpdate->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;
    nonInteractiveRoutineUpdate->status_message = "Routine ran by Google.";

    auto routineUpdateUnion =
        crosapi::mojom::DiagnosticsRoutineUpdateUnion::NewNoninteractiveUpdate(
            std::move(nonInteractiveRoutineUpdate));

    auto response = crosapi::mojom::DiagnosticsRoutineUpdate::New();
    response->progress_percent = 87;
    response->routine_update_union = std::move(routineUpdateUnion);

    // Set the return value for a call to GetAvailableRoutines.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRoutineUpdateResponse(std::move(response));

    // Set the expected passed parameters.
    base::Value::Dict expected_result;

    expected_result.Set("id", 123456);
    expected_result.Set(
        "command",
        static_cast<int32_t>(
            crosapi::mojom::DiagnosticsRoutineCommandEnum::kGetStatus));
    expected_result.Set("include_output", true);
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getRoutineUpdate() {
        const response =
          await chrome.os.diagnostics.getRoutineUpdate(
            {
              id: 123456,
              command: "status"
            }
          );
        chrome.test.assertEq(
          {
            progress_percent: 87,
            status: "ready",
            status_message: "Routine ran by Google."
          },
          response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       GetRoutineUpdateInteractiveSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto interactiveRoutineUpdate =
        crosapi::mojom::DiagnosticsInteractiveRoutineUpdate::New();
    interactiveRoutineUpdate->user_message =
        crosapi::mojom::DiagnosticsRoutineUserMessageEnum::kUnplugACPower;

    auto routineUpdateUnion =
        crosapi::mojom::DiagnosticsRoutineUpdateUnion::NewInteractiveUpdate(
            std::move(interactiveRoutineUpdate));

    auto response = crosapi::mojom::DiagnosticsRoutineUpdate::New();
    response->progress_percent = 50;
    response->output = "routine is running...";
    response->routine_update_union = std::move(routineUpdateUnion);

    // Set the return value for a call to GetAvailableRoutines.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRoutineUpdateResponse(std::move(response));

    // Set the expected passed parameters.
    base::Value::Dict expected_result;
    expected_result.Set("id", 654321);
    expected_result.Set(
        "command", static_cast<int32_t>(
                       crosapi::mojom::DiagnosticsRoutineCommandEnum::kRemove));
    expected_result.Set("include_output", true);
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function getRoutineUpdate() {
        const response =
          await chrome.os.diagnostics.getRoutineUpdate(
            {
              id: 654321,
              command: "remove",
            }
          );
        chrome.test.assertEq(
          {
            progress_percent: 50,
            output: "routine is running...",
            status: "waiting_user_action",
            status_message: "Waiting for user action. See user_message",
            user_message: "unplug_ac_power"
          },
          response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunAcPowerRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunAcPowerRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set(
        "expected_status",
        static_cast<int32_t>(
            crosapi::mojom::DiagnosticsAcPowerStatusEnum::kConnected));
    expected_result.Set("expected_power_type", "ac_power");

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kAcPower);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runAcPowerRoutine() {
        const response =
          await chrome.os.diagnostics.runAcPowerRoutine(
            {
              expected_status: "connected",
              expected_power_type: "ac_power",
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryCapacityRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryCapacityRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCapacity);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryCapacityRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryCapacityRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryChargeRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryChargeRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set("length_seconds", 1000);
    expected_result.Set("minimum_charge_percent_required", 1);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCharge);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryChargeRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryChargeRoutine(
            {
              length_seconds: 1000,
              minimum_charge_percent_required: 1
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryDischargeRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryDischargeRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set("length_seconds", 10);
    expected_result.Set("maximum_discharge_percent_allowed", 15);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryDischarge);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryDischargeRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryDischargeRoutine(
            {
              length_seconds: 10,
              maximum_discharge_percent_allowed: 15
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunBatteryHealthRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunBatteryHealthRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kBatteryHealth);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runBatteryHealthRoutine() {
        const response =
          await chrome.os.diagnostics.runBatteryHealthRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuCacheRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuCacheRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set("length_seconds", 120);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kCpuCache);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuCacheRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuCacheRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuFloatingPointAccuracyRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuFloatingPointAccuracyRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set("length_seconds", 120);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kFloatingPointAccuracy);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuFloatingPointAccuracyRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuFloatingPointAccuracyRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuPrimeSearchRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuPrimeSearchRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set("length_seconds", 120);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kPrimeSearch);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuPrimeSearchRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuPrimeSearchRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunCpuStressRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunCpuStressRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set("length_seconds", 120);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kCpuStress);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runCpuStressRoutine() {
        const response =
          await chrome.os.diagnostics.runCpuStressRoutine(
            {
              length_seconds: 120
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunDiskReadRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunDiskReadRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set(
        "type",
        static_cast<int32_t>(
            crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum::kLinearRead));
    expected_result.Set("length_seconds", 20);
    expected_result.Set("file_size_mb", 1000);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kDiskRead);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runDiskReadRoutine() {
        const response =
          await chrome.os.diagnostics.runDiskReadRoutine(
            {
                type: "linear",
                length_seconds: 20,
                file_size_mb: 1000
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunLanConnectivityRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunLanConnectivityRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kLanConnectivity);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runLanConnectivityRoutine() {
        const response =
          await chrome.os.diagnostics.runLanConnectivityRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunMemoryRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunMemoryRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kMemory);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runMemoryRoutine() {
        const response =
          await chrome.os.diagnostics.runMemoryRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunNvmeWearLevelRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunNvmeWearLevelRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    base::Value::Dict expected_result;
    expected_result.Set("wear_level_threshold", 80);

    // Set the expected runtime actions.
    fake_service_impl->SetExpectedLastPassedParameters(
        std::move(expected_result));
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kNvmeWearLevel);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runNvmeWearLevelRoutine() {
        const response =
          await chrome.os.diagnostics.runNvmeWearLevelRoutine(
            {
              wear_level_threshold: 80
            }
          );
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiBrowserTest,
                       RunSmartctlCheckRoutineSuccess) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If Diagnostics interface is not available on this version of ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Configure FakeDiagnosticsService.
  {
    auto expected_response =
        crosapi::mojom::DiagnosticsRunRoutineResponse::New();
    expected_response->id = 0;
    expected_response->status =
        crosapi::mojom::DiagnosticsRoutineStatusEnum::kReady;

    // Set the return value for a call to RunSmartctlCheckRoutine.
    auto fake_service_impl = std::make_unique<FakeDiagnosticsService>();
    fake_service_impl->SetRunRoutineResponse(std::move(expected_response));

    // Set the expected called routine.
    fake_service_impl->SetExpectedLastCalledRoutine(
        crosapi::mojom::DiagnosticsRoutineEnum::kSmartctlCheck);

    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function runSmartctlCheckRoutine() {
        const response =
          await chrome.os.diagnostics.runSmartctlCheckRoutine();
        chrome.test.assertEq({id: 0, status: "ready"}, response);
        chrome.test.succeed();
      }
    ]);
  )");
}

}  // namespace chromeos
