// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_

#include <vector>

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace chromeos::cloud_upload {

// The string conversions of chromeos::cloud_upload::mojom::UserAction.
const char kUserActionCancel[] = "cancel";
const char kUserActionUpload[] = "upload";

// Defines the web dialog used to help users upload Office files to the cloud.
class CloudUploadDialog : public SystemWebDialogDelegate {
 public:
  using UploadRequestCallback =
      base::OnceCallback<void(const std::string& action)>;

  CloudUploadDialog(const CloudUploadDialog&) = delete;
  CloudUploadDialog& operator=(const CloudUploadDialog&) = delete;

  // Creates and shows a new dialog for the cloud upload workflow. Returns true
  // if a new dialog has been effectively created.
  static bool Show(const std::vector<storage::FileSystemURL>& file_urls);

  void OnDialogClosed(const std::string& json_retval) override;

 protected:
  explicit CloudUploadDialog(const storage::FileSystemURL& file_url,
                             UploadRequestCallback callback);
  std::string GetDialogArgs() const override;
  ~CloudUploadDialog() override;
  bool ShouldShowCloseButton() const override;

 private:
  const storage::FileSystemURL file_url_;
  UploadRequestCallback callback_;
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
