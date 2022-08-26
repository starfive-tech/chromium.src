// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/probe_service_converters.h"

#include <unistd.h>
#include <utility>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"

namespace ash {
namespace converters {

namespace {

namespace cros_healthd = ::ash::cros_healthd;

cros_healthd::mojom::ProbeCategoryEnum Convert(
    crosapi::mojom::ProbeCategoryEnum input) {
  switch (input) {
    case crosapi::mojom::ProbeCategoryEnum::kUnknown:
      return cros_healthd::mojom::ProbeCategoryEnum::kUnknown;
    case crosapi::mojom::ProbeCategoryEnum::kBattery:
      return cros_healthd::mojom::ProbeCategoryEnum::kBattery;
    case crosapi::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices:
      return cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices;
    case crosapi::mojom::ProbeCategoryEnum::kCachedVpdData:
      return cros_healthd::mojom::ProbeCategoryEnum::kSystem;
    case crosapi::mojom::ProbeCategoryEnum::kCpu:
      return cros_healthd::mojom::ProbeCategoryEnum::kCpu;
    case crosapi::mojom::ProbeCategoryEnum::kTimezone:
      return cros_healthd::mojom::ProbeCategoryEnum::kTimezone;
    case crosapi::mojom::ProbeCategoryEnum::kMemory:
      return cros_healthd::mojom::ProbeCategoryEnum::kMemory;
    case crosapi::mojom::ProbeCategoryEnum::kBacklight:
      return cros_healthd::mojom::ProbeCategoryEnum::kBacklight;
    case crosapi::mojom::ProbeCategoryEnum::kFan:
      return cros_healthd::mojom::ProbeCategoryEnum::kFan;
    case crosapi::mojom::ProbeCategoryEnum::kStatefulPartition:
      return cros_healthd::mojom::ProbeCategoryEnum::kStatefulPartition;
    case crosapi::mojom::ProbeCategoryEnum::kBluetooth:
      return cros_healthd::mojom::ProbeCategoryEnum::kBluetooth;
    case crosapi::mojom::ProbeCategoryEnum::kSystem:
      return cros_healthd::mojom::ProbeCategoryEnum::kSystem;
  }
  NOTREACHED();
}

}  // namespace

namespace unchecked {

crosapi::mojom::ProbeErrorPtr UncheckedConvertPtr(
    cros_healthd::mojom::ProbeErrorPtr input) {
  return crosapi::mojom::ProbeError::New(Convert(input->type),
                                         std::move(input->msg));
}

crosapi::mojom::UInt64ValuePtr UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint64Ptr input) {
  return crosapi::mojom::UInt64Value::New(input->value);
}

crosapi::mojom::ProbeBatteryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryInfoPtr input) {
  return crosapi::mojom::ProbeBatteryInfo::New(
      Convert(input->cycle_count), Convert(input->voltage_now),
      std::move(input->vendor), std::move(input->serial_number),
      Convert(input->charge_full_design), Convert(input->charge_full),
      Convert(input->voltage_min_design), std::move(input->model_name),
      Convert(input->charge_now), Convert(input->current_now),
      std::move(input->technology), std::move(input->status),
      std::move(input->manufacture_date),
      ConvertProbePtr(std::move(input->temperature)));
}

crosapi::mojom::ProbeBatteryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BatteryResult::Tag::kBatteryInfo:
      return crosapi::mojom::ProbeBatteryResult::NewBatteryInfo(
          ConvertProbePtr(std::move(input->get_battery_info())));
    case cros_healthd::mojom::BatteryResult::Tag::kError:
      return crosapi::mojom::ProbeBatteryResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeNonRemovableBlockDeviceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr input) {
  return crosapi::mojom::ProbeNonRemovableBlockDeviceInfo::New(
      std::move(input->path), Convert(input->size), std::move(input->type),
      Convert(static_cast<uint32_t>(input->manufacturer_id)),
      std::move(input->name), base::NumberToString(input->serial),
      Convert(input->bytes_read_since_last_boot),
      Convert(input->bytes_written_since_last_boot),
      Convert(input->read_time_seconds_since_last_boot),
      Convert(input->write_time_seconds_since_last_boot),
      Convert(input->io_time_seconds_since_last_boot),
      ConvertProbePtr(std::move(input->discard_time_seconds_since_last_boot)));
}

crosapi::mojom::ProbeNonRemovableBlockDeviceResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::
        kBlockDeviceInfo:
      return crosapi::mojom::ProbeNonRemovableBlockDeviceResult::
          NewBlockDeviceInfo(
              ConvertPtrVector<
                  crosapi::mojom::ProbeNonRemovableBlockDeviceInfoPtr>(
                  std::move(input->get_block_device_info())));
    case cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::kError:
      return crosapi::mojom::ProbeNonRemovableBlockDeviceResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeCachedVpdInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::VpdInfoPtr input) {
  return crosapi::mojom::ProbeCachedVpdInfo::New(
      std::move(input->activate_date), std::move(input->sku_number),
      std::move(input->serial_number), std::move(input->model_name));
}

crosapi::mojom::ProbeCpuCStateInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuCStateInfoPtr input) {
  return crosapi::mojom::ProbeCpuCStateInfo::New(
      std::move(input->name), Convert(input->time_in_state_since_last_boot_us));
}

namespace {

uint64_t UserHz() {
  const long user_hz = sysconf(_SC_CLK_TCK);
  DCHECK(user_hz >= 0);
  return user_hz;
}

}  // namespace

crosapi::mojom::ProbeLogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input) {
  return UncheckedConvertPtr(std::move(input), UserHz());
}

crosapi::mojom::ProbeLogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input,
    uint64_t user_hz) {
  constexpr uint64_t kMillisecondsInSecond = 1000;
  uint64_t idle_time_user_hz = static_cast<uint64_t>(input->idle_time_user_hz);

  DCHECK(user_hz != 0);

  return crosapi::mojom::ProbeLogicalCpuInfo::New(
      Convert(input->max_clock_speed_khz),
      Convert(input->scaling_max_frequency_khz),
      Convert(input->scaling_current_frequency_khz),
      Convert(idle_time_user_hz * kMillisecondsInSecond / user_hz),
      ConvertPtrVector<crosapi::mojom::ProbeCpuCStateInfoPtr>(
          std::move(input->c_states)));
}

crosapi::mojom::ProbePhysicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PhysicalCpuInfoPtr input) {
  return crosapi::mojom::ProbePhysicalCpuInfo::New(
      std::move(input->model_name),
      ConvertPtrVector<crosapi::mojom::ProbeLogicalCpuInfoPtr>(
          std::move(input->logical_cpus)));
}

crosapi::mojom::ProbeCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuInfoPtr input) {
  return crosapi::mojom::ProbeCpuInfo::New(
      Convert(input->num_total_threads), Convert(input->architecture),
      ConvertPtrVector<crosapi::mojom::ProbePhysicalCpuInfoPtr>(
          std::move(input->physical_cpus)));
}

crosapi::mojom::ProbeCpuResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::CpuResult::Tag::kCpuInfo:
      return crosapi::mojom::ProbeCpuResult::NewCpuInfo(
          ConvertProbePtr(std::move(input->get_cpu_info())));
    case cros_healthd::mojom::CpuResult::Tag::kError:
      return crosapi::mojom::ProbeCpuResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeTimezoneInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneInfoPtr input) {
  return crosapi::mojom::ProbeTimezoneInfo::New(input->posix, input->region);
}

crosapi::mojom::ProbeTimezoneResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::TimezoneResult::Tag::kTimezoneInfo:
      return crosapi::mojom::ProbeTimezoneResult::NewTimezoneInfo(
          ConvertProbePtr(std::move(input->get_timezone_info())));
    case cros_healthd::mojom::TimezoneResult::Tag::kError:
      return crosapi::mojom::ProbeTimezoneResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeMemoryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryInfoPtr input) {
  return crosapi::mojom::ProbeMemoryInfo::New(
      Convert(input->total_memory_kib), Convert(input->free_memory_kib),
      Convert(input->available_memory_kib),
      Convert(static_cast<uint64_t>(input->page_faults_since_last_boot)));
}

crosapi::mojom::ProbeMemoryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::MemoryResult::Tag::kMemoryInfo:
      return crosapi::mojom::ProbeMemoryResult::NewMemoryInfo(
          ConvertProbePtr(std::move(input->get_memory_info())));
    case cros_healthd::mojom::MemoryResult::Tag::kError:
      return crosapi::mojom::ProbeMemoryResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeBacklightInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightInfoPtr input) {
  return crosapi::mojom::ProbeBacklightInfo::New(std::move(input->path),
                                                 Convert(input->max_brightness),
                                                 Convert(input->brightness));
}

crosapi::mojom::ProbeBacklightResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BacklightResult::Tag::kBacklightInfo:
      return crosapi::mojom::ProbeBacklightResult::NewBacklightInfo(
          ConvertPtrVector<crosapi::mojom::ProbeBacklightInfoPtr>(
              std::move(input->get_backlight_info())));
    case cros_healthd::mojom::BacklightResult::Tag::kError:
      return crosapi::mojom::ProbeBacklightResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeFanInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanInfoPtr input) {
  return crosapi::mojom::ProbeFanInfo::New(Convert(input->speed_rpm));
}

crosapi::mojom::ProbeFanResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::FanResult::Tag::kFanInfo:
      return crosapi::mojom::ProbeFanResult::NewFanInfo(
          ConvertPtrVector<crosapi::mojom::ProbeFanInfoPtr>(
              std::move(input->get_fan_info())));
    case cros_healthd::mojom::FanResult::Tag::kError:
      return crosapi::mojom::ProbeFanResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeStatefulPartitionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionInfoPtr input) {
  constexpr uint64_t k100MiB = 100 * 1024 * 1024;
  return crosapi::mojom::ProbeStatefulPartitionInfo::New(
      Convert(input->available_space / k100MiB * k100MiB),
      Convert(input->total_space));
}

crosapi::mojom::ProbeStatefulPartitionResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::StatefulPartitionResult::Tag::kPartitionInfo:
      return crosapi::mojom::ProbeStatefulPartitionResult::NewPartitionInfo(
          ConvertProbePtr(std::move(input->get_partition_info())));
    case cros_healthd::mojom::StatefulPartitionResult::Tag::kError:
      return crosapi::mojom::ProbeStatefulPartitionResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeBluetoothAdapterInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothAdapterInfoPtr input) {
  return crosapi::mojom::ProbeBluetoothAdapterInfo::New(
      std::move(input->name), std::move(input->address),
      Convert(input->powered), Convert(input->num_connected_devices));
}

crosapi::mojom::ProbeBluetoothResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BluetoothResult::Tag::kBluetoothAdapterInfo:
      return crosapi::mojom::ProbeBluetoothResult::NewBluetoothAdapterInfo(
          ConvertPtrVector<crosapi::mojom::ProbeBluetoothAdapterInfoPtr>(
              std::move(input->get_bluetooth_adapter_info())));
    case cros_healthd::mojom::BluetoothResult::Tag::kError:
      return crosapi::mojom::ProbeBluetoothResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeSystemInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsInfoPtr input) {
  return crosapi::mojom::ProbeSystemInfo::New(crosapi::mojom::ProbeOsInfo::New(
      std::move(input->oem_name),
      ConvertProbePtr(std::move(input->os_version))));
}

crosapi::mojom::ProbeOsVersionPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsVersionPtr input) {
  return crosapi::mojom::ProbeOsVersion::New(
      std::move(input->release_milestone), std::move(input->build_number),
      std::move(input->patch_number), std::move(input->release_channel));
}

std::pair<crosapi::mojom::ProbeCachedVpdInfoPtr,
          crosapi::mojom::ProbeSystemInfoPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemInfoPtr input) {
  return std::make_pair(ConvertProbePtr(std::move(input->vpd_info)),
                        ConvertProbePtr(std::move(input->os_info)));
}

std::pair<crosapi::mojom::ProbeCachedVpdResultPtr,
          crosapi::mojom::ProbeSystemResultPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::SystemResult::Tag::kSystemInfo: {
      auto output = ConvertProbePairPtr(std::move(input->get_system_info()));
      return std::make_pair(crosapi::mojom::ProbeCachedVpdResult::NewVpdInfo(
                                std::move(output.first)),
                            crosapi::mojom::ProbeSystemResult::NewSystemInfo(
                                std::move(output.second)));
    }
    case cros_healthd::mojom::SystemResult::Tag::kError: {
      auto system_error = ConvertProbePtr(std::move(input->get_error()));
      return std::make_pair(
          crosapi::mojom::ProbeCachedVpdResult::NewError(system_error.Clone()),
          crosapi::mojom::ProbeSystemResult::NewError(system_error.Clone()));
    }
  }
}

crosapi::mojom::ProbeTelemetryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TelemetryInfoPtr input) {
  auto system_result_output =
      ConvertProbePairPtr(std::move(input->system_result));

  return crosapi::mojom::ProbeTelemetryInfo::New(
      ConvertProbePtr(std::move(input->battery_result)),
      ConvertProbePtr(std::move(input->block_device_result)),
      std::move(system_result_output.first),
      ConvertProbePtr(std::move(input->cpu_result)),
      ConvertProbePtr(std::move(input->timezone_result)),
      ConvertProbePtr(std::move(input->memory_result)),
      ConvertProbePtr(std::move(input->backlight_result)),
      ConvertProbePtr(std::move(input->fan_result)),
      ConvertProbePtr(std::move(input->stateful_partition_result)),
      ConvertProbePtr(std::move(input->bluetooth_result)),
      std::move(system_result_output.second));
}

}  // namespace unchecked

crosapi::mojom::ProbeErrorType Convert(cros_healthd::mojom::ErrorType input) {
  switch (input) {
    case cros_healthd::mojom::ErrorType::kUnknown:
      return crosapi::mojom::ProbeErrorType::kUnknown;
    case cros_healthd::mojom::ErrorType::kFileReadError:
      return crosapi::mojom::ProbeErrorType::kFileReadError;
    case cros_healthd::mojom::ErrorType::kParseError:
      return crosapi::mojom::ProbeErrorType::kParseError;
    case cros_healthd::mojom::ErrorType::kSystemUtilityError:
      return crosapi::mojom::ProbeErrorType::kSystemUtilityError;
    case cros_healthd::mojom::ErrorType::kServiceUnavailable:
      return crosapi::mojom::ProbeErrorType::kServiceUnavailable;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeCpuArchitectureEnum Convert(
    cros_healthd::mojom::CpuArchitectureEnum input) {
  switch (input) {
    case cros_healthd::mojom::CpuArchitectureEnum::kUnknown:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kUnknown;
    case cros_healthd::mojom::CpuArchitectureEnum::kX86_64:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kX86_64;
    case cros_healthd::mojom::CpuArchitectureEnum::kAArch64:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kAArch64;
    case cros_healthd::mojom::CpuArchitectureEnum::kArmv7l:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kArmv7l;
  }
  NOTREACHED();
}

crosapi::mojom::BoolValuePtr Convert(bool input) {
  return crosapi::mojom::BoolValue::New(input);
}

crosapi::mojom::DoubleValuePtr Convert(double input) {
  return crosapi::mojom::DoubleValue::New(input);
}

crosapi::mojom::Int64ValuePtr Convert(int64_t input) {
  return crosapi::mojom::Int64Value::New(input);
}

crosapi::mojom::UInt32ValuePtr Convert(uint32_t input) {
  return crosapi::mojom::UInt32Value::New(input);
}

crosapi::mojom::UInt64ValuePtr Convert(uint64_t input) {
  return crosapi::mojom::UInt64Value::New(input);
}

std::vector<cros_healthd::mojom::ProbeCategoryEnum> ConvertCategoryVector(
    const std::vector<crosapi::mojom::ProbeCategoryEnum>& input) {
  std::vector<cros_healthd::mojom::ProbeCategoryEnum> output;
  for (const auto element : input) {
    output.push_back(Convert(element));
  }
  return output;
}

}  // namespace converters
}  // namespace ash
