// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"

#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

class ThreadWithCustomScheduler : public MainThread {
 public:
  explicit ThreadWithCustomScheduler(
      ThreadScheduler* scheduler,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : scheduler_(scheduler), task_runner_(std::move(task_runner)) {}
  ~ThreadWithCustomScheduler() override {}

  ThreadScheduler* Scheduler() override { return scheduler_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetDeprecatedTaskRunner()
      const override {
    return task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      MainThreadTaskRunnerRestricted) const override {
    return task_runner_;
  }

 private:
  ThreadScheduler* scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

ScopedSchedulerOverrider::ScopedSchedulerOverrider(
    ThreadScheduler* scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : main_thread_overrider_(std::make_unique<ThreadWithCustomScheduler>(
          scheduler,
          std::move(task_runner))) {}

ScopedSchedulerOverrider::~ScopedSchedulerOverrider() {}

}  // namespace blink
