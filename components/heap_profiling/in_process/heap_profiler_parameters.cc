// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_parameters.h"

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace heap_profiling {

namespace {

// Platform-specific parameter defaults.

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// Average 1M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 1'000'000;

// Default on iOS is equal to mean value of process uptime. Android is
// more similar to iOS than to Desktop.
constexpr int kDefaultCollectionIntervalInMinutes = 30;
#else
// Average 10M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 10'000'000;

// Default on desktop is once per day.
constexpr int kDefaultCollectionIntervalInMinutes = 24 * 60;
#endif

// Semicolon-separated list of process names to support. (More convenient than
// commas, which must be url-escaped in the --enable-features command line.)
constexpr base::FeatureParam<std::string> kSupportedProcesses{
    &kHeapProfilerReporting, "supported-processes", "browser"};

// Sets the chance that this client will report heap samples through a metrics
// provider if it's on the stable channel.
constexpr base::FeatureParam<double> kStableProbability {
  &kHeapProfilerReporting, "stable-probability",
#if BUILDFLAG(IS_ANDROID)
      // With stable-probability 0.01 we get about 4x as many records as before
      // https://crrev.com/c/3309878 landed in 98.0.4742.0, even with ARM64
      // disabled. This is too high a volume to process.
      0.0025
#else
      0.01
#endif
};

// Sets the chance that this client will report heap samples through a metrics
// provider if it's on a non-stable channel.
constexpr base::FeatureParam<double> kNonStableProbability{
    &kHeapProfilerReporting, "nonstable-probability", 0.5};

// Sets the mean heap sampling interval in bytes.
constexpr base::FeatureParam<int> kSamplingRateBytes{
    &kHeapProfilerReporting, "sampling-rate", kDefaultSamplingRateBytes};

// Sets the mean interval between snapshots.
constexpr base::FeatureParam<int> kCollectionIntervalMinutes{
    &kHeapProfilerReporting, "heap-profiler-collection-interval-minutes",
    kDefaultCollectionIntervalInMinutes};

// TODO(crbug.com/1327069): Replace the above FeatureParam's with a single
// "default-params" in the same JSON format as the following per-process
// parameters to reduce duplication.

// JSON-encoded parameter map that will override the default parameters for the
// browser process.
constexpr base::FeatureParam<std::string> kBrowserProcessParameters{
    &kHeapProfilerReporting, "browser-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for
// renderer processes.
constexpr base::FeatureParam<std::string> kRendererProcessParameters{
    &kHeapProfilerReporting, "renderer-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for the
// GPU process.
constexpr base::FeatureParam<std::string> kGPUProcessParameters{
    &kHeapProfilerReporting, "gpu-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for
// utility processes.
constexpr base::FeatureParam<std::string> kUtilityProcessParameters{
    &kHeapProfilerReporting, "utility-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for the
// network process.
constexpr base::FeatureParam<std::string> kNetworkProcessParameters{
    &kHeapProfilerReporting, "network-process-params", ""};

// Returns the string to use in the kSupportedProcesses feature param for
// `process_type`, or nullptr if the process is not supported.
const char* ProcessParamString(
    metrics::CallStackProfileParams::Process process_type) {
  using Process = metrics::CallStackProfileParams::Process;
  switch (process_type) {
    case Process::kBrowser:
      return "browser";
    case Process::kRenderer:
      return "renderer";
    case Process::kGpu:
      return "gpu";
    case Process::kUtility:
      return "utility";
    case Process::kNetworkService:
      return "networkService";
    case Process::kUnknown:
    default:
      // Profiler hasn't been tested in these process types.
      return nullptr;
  }
}

// Interprets `value` as a positive number of minutes, and writes the converted
// value to `result`. If `value` contains anything other than a positive
// integer, returns false to indicate a conversion failure.
bool ConvertCollectionInterval(const base::Value* value,
                               base::TimeDelta* result) {
  if (!value) {
    // Missing values are ok, so report success without updating `result`.
    return true;
  }
  if (value->is_int()) {
    const int minutes = value->GetInt();
    if (minutes > 0) {
      *result = base::Minutes(minutes);
      return true;
    }
  }
  return false;
}

}  // namespace

constexpr base::Feature kHeapProfilerReporting CONSTINIT{
    "HeapProfilerReporting", base::FEATURE_ENABLED_BY_DEFAULT};

// static
void HeapProfilerParameters::RegisterJSONConverter(
    base::JSONValueConverter<HeapProfilerParameters>* converter) {
  converter->RegisterBoolField("is-supported",
                               &HeapProfilerParameters::is_supported);
  converter->RegisterDoubleField("stable-probability",
                                 &HeapProfilerParameters::stable_probability);
  converter->RegisterDoubleField(
      "nonstable-probability", &HeapProfilerParameters::nonstable_probability);
  converter->RegisterIntField("sampling-rate-bytes",
                              &HeapProfilerParameters::sampling_rate_bytes);
  converter->RegisterCustomValueField(
      "collection-interval-minutes",
      &HeapProfilerParameters::collection_interval, &ConvertCollectionInterval);
}

HeapProfilerParameters GetDefaultHeapProfilerParameters() {
  return {
      .is_supported = false,
      // If a process overrides `is_supported`, use the following defaults.
      .stable_probability = kStableProbability.Get(),
      .nonstable_probability = kNonStableProbability.Get(),
      .sampling_rate_bytes = kSamplingRateBytes.Get(),
      .collection_interval = base::Minutes(kCollectionIntervalMinutes.Get()),
  };
}

HeapProfilerParameters GetHeapProfilerParametersForProcess(
    metrics::CallStackProfileParams::Process process_type) {
  HeapProfilerParameters params = GetDefaultHeapProfilerParameters();

  const char* process_string = ProcessParamString(process_type);
  if (!base::FeatureList::IsEnabled(kHeapProfilerReporting)) {
    // Heap profiling is never supported.
    params.is_supported = false;
  } else if (!process_string) {
    // This process type is never supported.
    params.is_supported = false;
  } else {
    const std::vector<std::string> supported_processes =
        base::SplitString(kSupportedProcesses.Get(), ";", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    params.is_supported = base::Contains(supported_processes, process_string);
  }

  // Override with per-process parameters if any are set.
  using Process = metrics::CallStackProfileParams::Process;
  std::string param_string;
  switch (process_type) {
    case Process::kBrowser:
      param_string = kBrowserProcessParameters.Get();
      break;
    case Process::kRenderer:
      param_string = kRendererProcessParameters.Get();
      break;
    case Process::kGpu:
      param_string = kGPUProcessParameters.Get();
      break;
    case Process::kUtility:
      param_string = kUtilityProcessParameters.Get();
      break;
    case Process::kNetworkService:
      param_string = kNetworkProcessParameters.Get();
      break;
    case Process::kUnknown:
    default:
      // Do nothing. Profiler hasn't been tested in these process types.
      break;
  }
  if (!param_string.empty()) {
    // Overwrite the defaults with any parameters set in `param_string`. Missing
    // parameters will not be touched.
    base::JSONValueConverter<HeapProfilerParameters> converter;
    absl::optional<base::Value> value =
        base::JSONReader::Read(param_string, base::JSON_ALLOW_TRAILING_COMMAS);
    if (!value || !converter.Convert(*value, &params)) {
      // Error reading JSON params. Disable the heap sampler. This will be
      // reported as HeapProfilerController logs
      // HeapProfiling.InProcess.Enabled.
      params.is_supported = false;
    }
  }

  return params;
}

}  // namespace heap_profiling
