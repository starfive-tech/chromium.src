// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"

namespace gl {
class GLShareGroup;
}

namespace android_webview {

class GpuServiceWebView;
class TaskQueueWebView;

// Implementation for gpu service objects accessor for command buffer WebView.
class DeferredGpuCommandService : public gpu::CommandBufferTaskExecutor {
 public:
  static DeferredGpuCommandService* GetInstance();

  DeferredGpuCommandService(const DeferredGpuCommandService&) = delete;
  DeferredGpuCommandService& operator=(const DeferredGpuCommandService&) =
      delete;

  // gpu::CommandBufferTaskExecutor implementation.
  bool ForceVirtualizedGLContexts() const override;
  bool ShouldCreateMemoryTracker() const override;
  std::unique_ptr<gpu::SingleTaskSequence> CreateSequence() override;
  void ScheduleOutOfOrderTask(base::OnceClosure task) override;
  void ScheduleDelayedWork(base::OnceClosure task) override;
  void PostNonNestableToClient(base::OnceClosure callback) override;
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  scoped_refptr<gl::GLShareGroup> GetShareGroup() override;

 protected:
  ~DeferredGpuCommandService() override;

 private:
  friend class TaskForwardingSequence;

  DeferredGpuCommandService(TaskQueueWebView* task_queue,
                            GpuServiceWebView* gpu_service);

  static DeferredGpuCommandService* CreateDeferredGpuCommandService();

  raw_ptr<TaskQueueWebView> task_queue_;
  raw_ptr<GpuServiceWebView> gpu_service_;
  scoped_refptr<gl::GLShareGroup> share_group_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_
