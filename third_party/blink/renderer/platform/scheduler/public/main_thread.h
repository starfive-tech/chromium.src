// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_H_

#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

// This class restricts access to the `GetTaskRunner`. Only authorized callees
// are allowed. Before you add a restriction consider using a frame or worker
// based API instead.
class MainThreadTaskRunnerRestricted {
 private:
  // Permitted users of `MainThread::GetTaskRunner`.
  friend class CachedStorageArea;
  friend class RendererResourceCoordinatorImpl;
  friend class ThreadedIconLoader;
  friend class V8WorkerMemoryReporter;

  MainThreadTaskRunnerRestricted() = default;
};

// The interface of a main thread in Blink.
//
// This class will has a restricted GetTaskRunner method.
//
class PLATFORM_EXPORT MainThread : public Thread {
 public:
  friend class Platform;  // For SetMainThread() and IsSimpleMainThread().
  friend class ScopedMainThreadOverrider;  // For SetMainThread().

  // Task runner for the main thread. This should only be used in
  // specific scenarios. Likely you want a frame or worker based task runner
  // instead.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      MainThreadTaskRunnerRestricted) const = 0;

 private:
  // For Platform and ScopedMainThreadOverrider. Return the thread object
  // previously set (if any).
  //
  // This is done this way because we need to be able to "override" the main
  // thread temporarily for ScopedTestingPlatformSupport.
  static std::unique_ptr<MainThread> SetMainThread(std::unique_ptr<MainThread>);

  // This is used to identify the actual Thread instance. This should be
  // used only in Platform, and other users should ignore this.
  virtual bool IsSimpleMainThread() const { return false; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_H_
