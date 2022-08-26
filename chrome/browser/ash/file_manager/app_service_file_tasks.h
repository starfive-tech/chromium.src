// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_APP_SERVICE_FILE_TASKS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_APP_SERVICE_FILE_TASKS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/file_manager/file_tasks.h"

class Profile;

namespace extensions {
struct EntryInfo;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager {
namespace file_tasks {

// Returns true if a file handler is enabled. Some handlers such as
// import-crostini-image can be disabled at runtime by enterprise policy.
bool FileHandlerIsEnabled(Profile* profile, const std::string& file_handler_id);

// Returns a profile that has App Service available. App Service doesn't exist
// in Incognito mode, so when the user opens a file from the downloads page
// within an Incognito browser, we will use the base profile instead. If neither
// the given profile nor the base profile have access to an available App
// Service, we return a nullptr.
Profile* GetProfileWithAppService(Profile* profile);

// Finds the app services tasks that can handle |entries|, appends them to
// |result_list|, and calls back to |callback|.
// Only support sharing at the moment.
void FindAppServiceTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         std::vector<FullTaskDescriptor>* result_list);

// Executes the specified task by app service.
void ExecuteAppServiceTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    const std::vector<std::string>& mime_types,
    FileTaskFinishedCallback done);

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_APP_SERVICE_FILE_TASKS_H_
