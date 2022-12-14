// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"

namespace content {

MockFileSystemAccessPermissionContext::MockFileSystemAccessPermissionContext() =
    default;
MockFileSystemAccessPermissionContext::
    ~MockFileSystemAccessPermissionContext() = default;

void MockFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
    const url::Origin& origin,
    PathType path_type,
    const base::FilePath& path,
    HandleType handle_type,
    ui::SelectFileDialog::Type dialog_type,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  ConfirmSensitiveEntryAccess_(origin, path_type, path, handle_type,
                               dialog_type, frame_id, callback);
}

void MockFileSystemAccessPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<FileSystemAccessWriteItem> item,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  PerformAfterWriteChecks_(item.get(), frame_id, callback);
}

}  // namespace content
