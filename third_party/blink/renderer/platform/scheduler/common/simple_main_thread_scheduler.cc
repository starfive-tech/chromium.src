// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/simple_main_thread_scheduler.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink::scheduler {

SimpleMainThreadScheduler::SimpleMainThreadScheduler() = default;

SimpleMainThreadScheduler::~SimpleMainThreadScheduler() = default;

void SimpleMainThreadScheduler::Shutdown() {}

bool SimpleMainThreadScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

void SimpleMainThreadScheduler::PostIdleTask(const base::Location& location,
                                             Thread::IdleTask task) {}

void SimpleMainThreadScheduler::PostDelayedIdleTask(const base::Location&,
                                                    base::TimeDelta delay,
                                                    Thread::IdleTask) {}

void SimpleMainThreadScheduler::PostNonNestableIdleTask(
    const base::Location& location,
    Thread::IdleTask task) {}

void SimpleMainThreadScheduler::AddRAILModeObserver(
    RAILModeObserver* observer) {}

void SimpleMainThreadScheduler::RemoveRAILModeObserver(
    RAILModeObserver const* observer) {}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleMainThreadScheduler::V8TaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleMainThreadScheduler::DeprecatedDefaultTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SimpleMainThreadScheduler::NonWakingTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

std::unique_ptr<WebAgentGroupScheduler>
SimpleMainThreadScheduler::CreateAgentGroupScheduler() {
  return nullptr;
}

WebAgentGroupScheduler*
SimpleMainThreadScheduler::GetCurrentAgentGroupScheduler() {
  return nullptr;
}

base::TimeTicks
SimpleMainThreadScheduler::MonotonicallyIncreasingVirtualTime() {
  return base::TimeTicks::Now();
}

void SimpleMainThreadScheduler::AddTaskObserver(
    base::TaskObserver* task_observer) {}

void SimpleMainThreadScheduler::RemoveTaskObserver(
    base::TaskObserver* task_observer) {}

MainThreadScheduler* SimpleMainThreadScheduler::ToMainThreadScheduler() {
  return this;
}

std::unique_ptr<MainThreadScheduler::RendererPauseHandle>
SimpleMainThreadScheduler::PauseScheduler() {
  return nullptr;
}

}  // namespace blink::scheduler
