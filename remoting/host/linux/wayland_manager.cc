// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_manager.h"

#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

WaylandManager::WaylandManager() {}

WaylandManager::~WaylandManager() {}

// static
WaylandManager* WaylandManager::Get() {
  static base::NoDestructor<WaylandManager> instance;
  return instance.get();
}

void WaylandManager::Init(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
  wayland_connection_ =
      std::make_unique<WaylandConnection>(getenv("WAYLAND_DISPLAY"));
}

void WaylandManager::AddCapturerMetadataCallback(
    DesktopMetadataCallback callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::AddCapturerMetadataCallback,
                       base::Unretained(this),
                       base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                                          std::move(callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  capturer_metadata_callbacks_.AddUnsafe(std::move(callback));
}

void WaylandManager::OnDesktopCapturerMetadata(
    webrtc::DesktopCaptureMetadata metadata) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnDesktopCapturerMetadata,
                                  base::Unretained(this), std::move(metadata)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  capturer_metadata_callbacks_.Notify(std::move(metadata));
}

DesktopDisplayInfo WaylandManager::GetCurrentDisplayInfo() {
  return wayland_connection_->GetCurrentDisplayInfo();
}

}  // namespace remoting
