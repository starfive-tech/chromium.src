// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_DELEGATE_BASE_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_DELEGATE_BASE_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/metrics/event_driven_telemetry_sampler_pool.h"
#include "components/reporting/metrics/metric_data_collector.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"

namespace reporting::metrics {

// Base class for the delegate used by the `MetricReportingManager` to
// initialize metric related components.
class MetricReportingManagerDelegateBase {
 public:
  MetricReportingManagerDelegateBase() = default;
  MetricReportingManagerDelegateBase(
      const MetricReportingManagerDelegateBase& other) = delete;
  MetricReportingManagerDelegateBase& operator=(
      const MetricReportingManagerDelegateBase& other) = delete;
  virtual ~MetricReportingManagerDelegateBase() = default;

  // Creates a new `MetricReportQueue` that can be used towards metrics
  // reporting.
  virtual std::unique_ptr<MetricReportQueue> CreateMetricReportQueue(
      EventType event_type,
      Destination destination,
      Priority priority);

  // Creates a new `MetricReportQueue` for periodic uploads. The rate is
  // controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<MetricReportQueue> CreatePeriodicUploadReportQueue(
      EventType event_type,
      Destination destination,
      Priority priority,
      ReportingSettings* reporting_settings,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms = 1);

  // Creates a new collector for periodic metric collection. The rate is
  // controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<CollectorBase> CreatePeriodicCollector(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms);

  // Creates a new collector for one shot metric collection. The rate is
  // controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<CollectorBase> CreateOneShotCollector(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value);

  // Creates a new event collector for periodic event data collection. The rate
  // is controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<CollectorBase> CreatePeriodicEventCollector(
      Sampler* sampler,
      std::unique_ptr<EventDetector> event_detector,
      EventDrivenTelemetrySamplerPool* sampler_pool,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms);

  // Creates a new event observer manager to manage events reporting. The rate
  // is controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<MetricEventObserverManager>
  CreateEventObserverManager(
      std::unique_ptr<MetricEventObserver> event_observer,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      EventDrivenTelemetrySamplerPool* sampler_pool);

  // Checks for profile affiliation and returns true if affiliated. False
  // otherwise.
  virtual bool IsAffiliated(Profile* profile) const;

  // Returns the delay interval used with `MetricReportingManager`
  // initialization.
  base::TimeDelta GetInitDelay() const;

  // Returns the delay interval used with initial record uploads.
  base::TimeDelta GetInitialUploadDelay() const;
};

}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_DELEGATE_BASE_H_
