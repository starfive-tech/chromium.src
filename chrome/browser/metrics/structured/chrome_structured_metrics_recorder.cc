// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"

#include <stdint.h>

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/structured/histogram_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/metrics/structured/ash_structured_metrics_recorder.h"  // nogncheck
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/task/current_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/metrics/structured/lacros_structured_metrics_recorder.h"  // nogncheck
#endif
namespace metrics {
namespace structured {
namespace {

// Platforms for which the StructuredMetricsClient will be initialized for.
enum class StructuredMetricsPlatform {
  kUninitialized = 0,
  kAshChrome = 1,
  kLacrosChrome = 2,
};

// Logs initialization of Structured Metrics as a record.
void LogInitializationInStructuredMetrics(StructuredMetricsPlatform platform) {
  events::v2::structured_metrics::Initialization()
      .SetPlatform(static_cast<int64_t>(platform))
      .Record();
}

}  // namespace

ChromeStructuredMetricsRecorder::ChromeStructuredMetricsRecorder() {
// TODO(jongahn): Make a static factory class and pass it into ctor.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  delegate_ = std::make_unique<AshStructuredMetricsRecorder>();
  StructuredMetricsClient::Get()->SetDelegate(this);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  delegate_ = std::make_unique<LacrosStructuredMetricsRecorder>();
  StructuredMetricsClient::Get()->SetDelegate(this);
#endif
}

ChromeStructuredMetricsRecorder::~ChromeStructuredMetricsRecorder() = default;

// static
ChromeStructuredMetricsRecorder* ChromeStructuredMetricsRecorder::Get() {
  static base::NoDestructor<ChromeStructuredMetricsRecorder> chrome_recorder;
  return chrome_recorder.get();
}

void ChromeStructuredMetricsRecorder::Initialize() {
// TODO(jongahn): Refactor this by making Initialize() part of the interface.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* ash_recorder =
      static_cast<AshStructuredMetricsRecorder*>(delegate_.get());
  ash_recorder->Initialize();
  LogInitializationInStructuredMetrics(StructuredMetricsPlatform::kAshChrome);

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_recorder =
      static_cast<LacrosStructuredMetricsRecorder*>(delegate_.get());

  // Ensure that the sequence is the ui thread.
  DCHECK(base::CurrentUIThread::IsSet());
  lacros_recorder->SetSequence(base::SequencedTaskRunnerHandle::Get());
  LogInitializationInStructuredMetrics(
      StructuredMetricsPlatform::kLacrosChrome);
#endif
}

void ChromeStructuredMetricsRecorder::RecordEvent(Event&& event) {
  DCHECK(IsReadyToRecord());
  delegate_->RecordEvent(std::move(event));
}

bool ChromeStructuredMetricsRecorder::IsReadyToRecord() const {
  return delegate_->IsReadyToRecord();
}

}  // namespace structured
}  // namespace metrics
