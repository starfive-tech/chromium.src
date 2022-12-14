// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SIMPLE_MAIN_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SIMPLE_MAIN_THREAD_SCHEDULER_H_

#include <memory>

#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"

namespace blink {
namespace scheduler {

// ThreadScheduler implementation that should be used when you don't have a
// dedicated ThreadScheduler instance. This is essentially a scheduler doesn't
// really schedule anything -- it just runs tasks as they come.
//
// This scheduler does not implement several features (see comments at each
// function). If you invoke a Blink functionality that relies on an
// unimplemented scheduler function, you might observe crashes or misbehavior.
// Apparently we don't rely on those missing features at least for things that
// use this scheduler.

class SimpleMainThreadScheduler : public MainThreadScheduler {
 public:
  SimpleMainThreadScheduler();
  SimpleMainThreadScheduler(const SimpleMainThreadScheduler&) = delete;
  SimpleMainThreadScheduler& operator=(const SimpleMainThreadScheduler&) =
      delete;
  ~SimpleMainThreadScheduler() override;

  // Do nothing.
  void Shutdown() override;

  // Always return false.
  bool ShouldYieldForHighPriorityWork() override;

  // Those tasks are simply ignored (we assume there's no idle period).
  void PostIdleTask(const base::Location&, Thread::IdleTask) override;
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta delay,
                           Thread::IdleTask) override;
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override;

  // Do nothing (the observer won't get notified).
  void AddRAILModeObserver(RAILModeObserver*) override;

  // Do nothing.
  void RemoveRAILModeObserver(RAILModeObserver const*) override;

  // Return the thread task runner (there's no separate task runner for them).
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override;

  // Unsupported. Return nullptr.
  std::unique_ptr<WebAgentGroupScheduler> CreateAgentGroupScheduler() override;

  // Return nullptr
  WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() override;

  // Return the current time.
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override;

  // Unsupported. The observer won't get called. May break some functionalities
  // that rely on the task observer.
  void AddTaskObserver(base::TaskObserver*) override;
  void RemoveTaskObserver(base::TaskObserver*) override;

  // Return nullptr.
  MainThreadScheduler* ToMainThreadScheduler() override;

  void SetV8Isolate(v8::Isolate* isolate) override {}
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SIMPLE_MAIN_THREAD_SCHEDULER_H_
