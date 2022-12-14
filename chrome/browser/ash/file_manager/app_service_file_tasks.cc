// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

using extensions::api::file_manager_private::Verb;

namespace {
TaskType GetTaskType(apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kArc:
      return TASK_TYPE_ARC_APP;
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      return TASK_TYPE_WEB_APP;
    case apps::AppType::kChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kStandaloneBrowserExtension:
      // Chrome apps and Extensions both get called file_handler, even though
      // extensions really have file_browser_handler. It doesn't matter anymore
      // because both are executed through App Service, which can tell the
      // difference itself.
      return TASK_TYPE_FILE_HANDLER;
    case apps::AppType::kCrostini:
      return TASK_TYPE_CROSTINI_APP;
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kMacOs:
    case apps::AppType::kPluginVm:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
      return TASK_TYPE_UNKNOWN;
  }
}

const char kImportCrostiniImageHandlerId[] = "import-crostini-image";
const char kInstallLinuxPackageHandlerId[] = "install-linux-package";

}  // namespace

bool FileHandlerIsEnabled(Profile* profile,
                          const std::string& file_handler_id) {
  // Crostini deb files and backup files can be disabled by policy.
  if (file_handler_id == kInstallLinuxPackageHandlerId) {
    return crostini::CrostiniFeatures::Get()->IsRootAccessAllowed(profile);
  }
  if (file_handler_id == kImportCrostiniImageHandlerId) {
    return crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile);
  }
  return true;
}

// Check if the file URLs can be mapped to a path inside VMs for
// GuestOS apps to access.
bool FilesCanBeSharedToVm(Profile* profile, std::vector<GURL> file_urls) {
  storage::FileSystemContext* file_system_context =
      util::GetFileManagerFileSystemContext(profile);
  base::FilePath placeholder_vm_mount("/");
  base::FilePath not_used;
  for (const GURL& file_url : file_urls) {
    if (!file_manager::util::ConvertFileSystemURLToPathInsideVM(
            profile, file_system_context->CrackURLInFirstPartyContext(file_url),
            placeholder_vm_mount,
            /*map_crostini_home=*/false, &not_used)) {
      return false;
    }
  }
  return true;
}

Profile* GetProfileWithAppService(Profile* profile) {
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return profile;
  } else {
    if (profile->IsOffTheRecord()) {
      return profile->GetOriginalProfile();
    } else {
      LOG(WARNING) << "Unexpected profile type";
      return nullptr;
    }
  }
}

void FindAppServiceTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         std::vector<FullTaskDescriptor>* result_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(entries.size(), file_urls.size());
  // App Service uses the file extension in the URL for file_handlers for Web
  // Apps.
#if DCHECK_IS_ON()
  for (const GURL& url : file_urls)
    DCHECK(url.is_valid());
#endif  // DCHECK_IS_ON()

  // WebApps only have full support for files backed by inodes, so tasks
  // provided by most Web Apps will be skipped if any non-native files are
  // present. "System" Web Apps are an exception: we have more control over what
  // they can do, so tasks provided by System Web Apps are the only ones
  // permitted at present. See https://crbug.com/1079065.
  bool has_non_native_file = false;
  bool has_pdf_file = false;
  for (const auto& entry : entries) {
    if (!has_non_native_file &&
        util::IsUnderNonNativeLocalPath(profile, entry.path))
      has_non_native_file = true;
    if (!has_pdf_file && entry.path.MatchesExtension(".pdf"))
      has_pdf_file = true;
  }

  // App Service doesn't exist in Incognito mode but we still want to find
  // handlers to open a download from its notification from Incognito mode. Use
  // the base profile in these cases (see crbug.com/1111695).
  Profile* profile_with_app_service = GetProfileWithAppService(profile);
  if (!profile_with_app_service) {
    LOG(WARNING) << "Unexpected profile type";
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_with_app_service);

  bool files_shareable_to_vm =
      FilesCanBeSharedToVm(profile_with_app_service, file_urls);

  std::vector<apps::IntentFilePtr> intent_files;
  intent_files.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    auto file = std::make_unique<apps::IntentFile>(file_urls.at(i));
    file->mime_type = entries[i].mime_type;
    file->is_directory = entries[i].is_directory;
    intent_files.push_back(std::move(file));
  }
  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      proxy->GetAppsForFiles(std::move(intent_files));

  std::vector<apps::AppType> supported_app_types = {
      apps::AppType::kWeb,
      apps::AppType::kSystemWeb,
      apps::AppType::kChromeApp,
      apps::AppType::kExtension,
      apps::AppType::kStandaloneBrowserChromeApp,
      apps::AppType::kStandaloneBrowserExtension};
  if (ash::features::ShouldArcAndGuestOsFileTasksUseAppService()) {
    supported_app_types.push_back(apps::AppType::kArc);
    supported_app_types.push_back(apps::AppType::kCrostini);
  }
  for (auto& launch_entry : intent_launch_info) {
    auto app_type = proxy->AppRegistryCache().GetAppType(launch_entry.app_id);
    if (!base::Contains(supported_app_types, app_type)) {
      continue;
    }

    if (app_type == apps::AppType::kWeb ||
        app_type == apps::AppType::kSystemWeb) {
      // Media app and other SWAs can handle "non-native" files.
      if (has_non_native_file &&
          !web_app::IsSystemAppIdWithFileHandlers(launch_entry.app_id)) {
        continue;
      }

      // "Hide" the media app task (i.e. skip the rest of this loop which would
      // add it as a handler) when the flag to handle PDF is off.
      if (launch_entry.app_id == web_app::kMediaAppId &&
          ((!base::FeatureList::IsEnabled(ash::features::kMediaAppHandlesPdf) &&
            has_pdf_file)))
        continue;

      // Check the origin trial and feature flag for file handling in web apps.
      // TODO(1240018): Remove when this feature is fully launched. This check
      // will not work for lacros web apps.
      web_app::WebAppProvider* provider =
          web_app::WebAppProvider::GetDeprecated(profile_with_app_service);
      web_app::OsIntegrationManager& os_integration_manager =
          provider->os_integration_manager();
      if (!os_integration_manager.IsFileHandlingAPIAvailable(
              launch_entry.app_id))
        continue;
    }

    if (app_type == apps::AppType::kChromeApp ||
        app_type == apps::AppType::kExtension) {
      if (profile->IsOffTheRecord() &&
          !extensions::util::IsIncognitoEnabled(launch_entry.app_id, profile))
        continue;
      if (!FileHandlerIsEnabled(profile_with_app_service,
                                launch_entry.activity_name))
        continue;
    }

    if (app_type == apps::AppType::kCrostini && !files_shareable_to_vm) {
      continue;
    }

    constexpr int kIconSize = 32;
    GURL icon_url =
        apps::AppIconSource::GetIconURL(launch_entry.app_id, kIconSize);
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(launch_entry.app_id, GetTaskType(app_type),
                       launch_entry.activity_name),
        launch_entry.activity_label, Verb::VERB_NONE, icon_url,
        /* is_default=*/false,
        // TODO(petermarshall): Handle the rest of the logic from FindWebTasks()
        // e.g. prioritise non-generic handlers.
        /* is_generic=*/launch_entry.is_generic_file_handler,
        /* is_file_extension_match=*/launch_entry.is_file_extension_match));
  }
}

void ExecuteAppServiceTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    const std::vector<std::string>& mime_types,
    FileTaskFinishedCallback done) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(file_system_urls.size(), mime_types.size());

  // App Service doesn't exist in Incognito mode but apps can be
  // launched (ie. default handler to open a download from its
  // notification) from Incognito mode. Use the base profile in these
  // cases (see crbug.com/1111695).
  Profile* profile_with_app_service = GetProfileWithAppService(profile);
  if (!profile_with_app_service) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Unexpected profile type");
    return;
  }

  std::vector<GURL> file_urls;
  std::vector<apps::IntentFilePtr> intent_files;
  file_urls.reserve(file_system_urls.size());
  intent_files.reserve(file_system_urls.size());
  for (size_t i = 0; i < file_system_urls.size(); i++) {
    file_urls.push_back(file_system_urls[i].ToGURL());

    auto file =
        std::make_unique<apps::IntentFile>(file_system_urls[i].ToGURL());
    file->mime_type = mime_types.at(i);
    intent_files.push_back(std::move(file));
  }

  if (ash::features::ShouldArcAndGuestOsFileTasksUseAppService()) {
    DCHECK(task.task_type == TASK_TYPE_ARC_APP ||
           task.task_type == TASK_TYPE_WEB_APP ||
           task.task_type == TASK_TYPE_FILE_HANDLER ||
           task.task_type == TASK_TYPE_CROSTINI_APP);
  } else {
    DCHECK(task.task_type == TASK_TYPE_WEB_APP ||
           task.task_type == TASK_TYPE_FILE_HANDLER);
  }
  apps::IntentPtr intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView, std::move(intent_files));
  intent->activity_name = task.action_id;

  if (base::FeatureList::IsEnabled(apps::kAppServiceLaunchWithoutMojom)) {
    apps::AppServiceProxyFactory::GetForProfile(profile_with_app_service)
        ->LaunchAppWithIntent(
            task.app_id, ui::EF_NONE, std::move(intent),
            apps::LaunchSource::kFromFileManager,
            std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId),
            base::BindOnce(
                [](FileTaskFinishedCallback done, TaskType task_type,
                   bool success) {
                  if (!success) {
                    std::move(done).Run(extensions::api::file_manager_private::
                                            TASK_RESULT_FAILED,
                                        "");
                  } else if (task_type == TASK_TYPE_WEB_APP) {
                    // TODO(benwells): return the correct code here, depending
                    // on how the app will be opened in multiprofile.
                    std::move(done).Run(extensions::api::file_manager_private::
                                            TASK_RESULT_OPENED,
                                        "");
                  } else {
                    std::move(done).Run(extensions::api::file_manager_private::
                                            TASK_RESULT_MESSAGE_SENT,
                                        "");
                  }
                },
                std::move(done), task.task_type));
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile_with_app_service)
        ->LaunchAppWithIntent(
            task.app_id, ui::EF_NONE, apps::ConvertIntentToMojomIntent(intent),
            apps::mojom::LaunchSource::kFromFileManager,
            apps::MakeWindowInfo(display::kDefaultDisplayId),
            base::BindOnce(
                [](FileTaskFinishedCallback done, TaskType task_type,
                   bool success) {
                  if (!success) {
                    std::move(done).Run(extensions::api::file_manager_private::
                                            TASK_RESULT_FAILED,
                                        "");
                  } else if (task_type == TASK_TYPE_WEB_APP) {
                    // TODO(benwells): return the correct code here, depending
                    // on how the app will be opened in multiprofile.
                    std::move(done).Run(extensions::api::file_manager_private::
                                            TASK_RESULT_OPENED,
                                        "");
                  } else {
                    std::move(done).Run(extensions::api::file_manager_private::
                                            TASK_RESULT_MESSAGE_SENT,
                                        "");
                  }
                },
                std::move(done), task.task_type));
  }
}

}  // namespace file_tasks
}  // namespace file_manager
