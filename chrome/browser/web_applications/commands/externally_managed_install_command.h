// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNALLY_MANAGED_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNALLY_MANAGED_INSTALL_COMMAND_H_

#include <memory>
#include <variant>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace content {
class WebContents;
}

namespace web_app {

class NoopLock;
class WebAppDataRetriever;
class WebAppInstallFinalizer;

// Command to install web_apps from param by the ExternallyInstalledAppsManager
class ExternallyManagedInstallCommand : public WebAppCommand {
 public:
  ExternallyManagedInstallCommand(
      const ExternalInstallOptions& external_install_options,
      OnceInstallCallback callback,
      base::WeakPtr<content::WebContents> contents,
      WebAppInstallFinalizer* install_finalizer,
      std::unique_ptr<WebAppDataRetriever> data_retriever);
  ~ExternallyManagedInstallCommand() override;

  Lock& lock() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  base::Value ToDebugValue() const override;

  void SetOnLockUpgradedCallbackForTesting(base::OnceClosure callback);

 private:
  void Abort(webapps::InstallResultCode code);

  void OnGetWebAppInstallInfoInCommand(
      std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    const GURL& manifest_url,
                                    bool valid_manifest_for_web_app,
                                    bool is_installable);
  void OnIconsRetrievedUpgradeLock(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);

  void OnLockUpgradedFinalizeInstall();

  void OnInstallFinalized(const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);

  std::unique_ptr<NoopLock> noop_lock_;

  std::unique_ptr<AppLock> app_lock_ = nullptr;

  WebAppInstallParams install_params_;
  webapps::WebappInstallSource install_surface_;
  OnceInstallCallback install_callback_;

  base::WeakPtr<content::WebContents> web_contents_;
  base::raw_ptr<WebAppInstallFinalizer> install_finalizer_;

  bool bypass_service_worker_check_ = false;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  InstallErrorLogEntry install_error_log_entry_;

  AppId app_id_;

  base::OnceClosure on_lock_upgraded_callback_for_testing_;

  base::WeakPtrFactory<ExternallyManagedInstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_EXTERNALLY_MANAGED_INSTALL_COMMAND_H_
