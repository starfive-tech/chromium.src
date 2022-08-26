// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/support_tool_resources.h"
#include "chrome/grit/support_tool_resources_map.h"
#include "components/feedback/pii_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace {

content::WebUIDataSource* CreateSupportToolHTMLSource(const GURL& url) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISupportToolHost);

  source->AddString("caseId", GetSupportCaseIDFromURL(url));

  webui::SetupWebUIDataSource(
      source, base::make_span(kSupportToolResources, kSupportToolResourcesSize),
      IDR_SUPPORT_TOOL_SUPPORT_TOOL_CONTAINER_HTML);

  source->AddResourcePath("url-generator",
                          IDR_SUPPORT_TOOL_URL_GENERATOR_CONTAINER_HTML);

  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages related to Support Tool.
class SupportToolMessageHandler : public content::WebUIMessageHandler,
                                  public ui::SelectFileDialog::Listener {
 public:
  SupportToolMessageHandler() = default;

  SupportToolMessageHandler(const SupportToolMessageHandler&) = delete;
  SupportToolMessageHandler& operator=(const SupportToolMessageHandler&) =
      delete;

  ~SupportToolMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleGetEmailAddresses(const base::Value::List& args);

  void HandleGetDataCollectors(const base::Value::List& args);

  void HandleGetAllDataCollectors(const base::Value::List& args);

  void HandleStartDataCollection(const base::Value::List& args);

  void HandleCancelDataCollection(const base::Value::List& args);

  void HandleStartDataExport(const base::Value::List& args);

  void HandleShowExportedDataInFolder(const base::Value::List& args);

  void HandleGenerateCustomizedURL(const base::Value::List& args);

  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  void FileSelectionCanceled(void* params) override;

 private:
  base::Value::List GetAccountsList();

  void OnDataCollectionDone(const PIIMap& detected_pii,
                            std::set<SupportToolError> errors);

  void OnDataExportDone(base::FilePath path, std::set<SupportToolError> errors);

  std::set<feedback::PIIType> selected_pii_to_keep_;
  base::FilePath data_path_;
  std::unique_ptr<SupportToolHandler> handler_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  base::WeakPtrFactory<SupportToolMessageHandler> weak_ptr_factory_{this};
};

SupportToolMessageHandler::~SupportToolMessageHandler() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void SupportToolMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getEmailAddresses",
      base::BindRepeating(&SupportToolMessageHandler::HandleGetEmailAddresses,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getDataCollectors",
      base::BindRepeating(&SupportToolMessageHandler::HandleGetDataCollectors,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getAllDataCollectors",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleGetAllDataCollectors,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "startDataCollection",
      base::BindRepeating(&SupportToolMessageHandler::HandleStartDataCollection,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "cancelDataCollection",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleCancelDataCollection,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "startDataExport",
      base::BindRepeating(&SupportToolMessageHandler::HandleStartDataExport,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "showExportedDataInFolder",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleShowExportedDataInFolder,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "generateCustomizedURL",
      base::BindRepeating(
          &SupportToolMessageHandler::HandleGenerateCustomizedURL,
          weak_ptr_factory_.GetWeakPtr()));
}

base::Value::List SupportToolMessageHandler::GetAccountsList() {
  Profile* profile = Profile::FromWebUI(web_ui());
  base::Value::List account_list;

  // Guest mode does not have a primary account (or an IdentityManager).
  if (profile->IsGuestSession())
    return account_list;

  for (const auto& account : signin_ui_util::GetOrderedAccountsForDisplay(
           profile, /*restrict_to_accounts_eligible_for_sync=*/false)) {
    if (!account.IsEmpty())
      account_list.Append(account.email);
  }
  return account_list;
}

void SupportToolMessageHandler::HandleGetEmailAddresses(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetAccountsList());
}

void SupportToolMessageHandler::HandleGetDataCollectors(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  std::string module_query;
  net::GetValueForKeyInQuery(web_ui()->GetWebContents()->GetURL(),
                             support_tool_ui::kModuleQuery, &module_query);

  ResolveJavascriptCallback(callback_id,
                            GetDataCollectorItemsInQuery(module_query));
}

void SupportToolMessageHandler::HandleGetAllDataCollectors(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetAllDataCollectors());
}

// Starts data collection with the issue details and selected set of data
// collectors that are sent from UI. Returns the result to UI in the format UI
// accepts.
void SupportToolMessageHandler::HandleStartDataCollection(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict* issue_details = args[1].GetIfDict();
  DCHECK(issue_details);
  const base::Value::List* data_collectors = args[2].GetIfList();
  DCHECK(data_collectors);
  std::set<support_tool::DataCollectorType> included_data_collectors =
      GetIncludedDataCollectorTypes(data_collectors);
  // Send error message to UI if there's no selected data collectors to include.
  if (included_data_collectors.empty()) {
    ResolveJavascriptCallback(
        callback_id,
        GetStartDataCollectionResult(
            /*success=*/false,
            /*error_message=*/"No data collector selected. Please select at "
                              "least one data collector."));
    return;
  }
  this->handler_ =
      GetSupportToolHandler(*issue_details->FindString("caseId"),
                            *issue_details->FindString("emailAddress"),
                            *issue_details->FindString("issueDescription"),
                            GetIncludedDataCollectorTypes(data_collectors));
  this->handler_->CollectSupportData(
      base::BindOnce(&SupportToolMessageHandler::OnDataCollectionDone,
                     weak_ptr_factory_.GetWeakPtr()));
  ResolveJavascriptCallback(
      callback_id, GetStartDataCollectionResult(
                       /*success=*/true, /*error_message=*/std::string()));
}

void SupportToolMessageHandler::OnDataCollectionDone(
    const PIIMap& detected_pii,
    std::set<SupportToolError> errors) {
  AllowJavascript();
  FireWebUIListener("data-collection-completed",
                    GetDetectedPIIDataItems(detected_pii));
}

void SupportToolMessageHandler::HandleCancelDataCollection(
    const base::Value::List& args) {
  AllowJavascript();
  // Deleting the SupportToolHandler object will stop data collection.
  this->handler_.reset();
  FireWebUIListener("data-collection-cancelled");
}

void SupportToolMessageHandler::HandleStartDataExport(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List* pii_items = args[0].GetIfList();
  DCHECK(pii_items);
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  selected_pii_to_keep_ = GetPIITypesToKeep(pii_items);

  AllowJavascript();
  content::WebContents* web_contents = web_ui()->GetWebContents();
  gfx::NativeWindow owning_window =
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::kNullNativeWindow;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));

  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(web_contents->GetBrowserContext());
  base::FilePath suggested_path = download_prefs->SaveFilePath();

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      /*title=*/std::u16string(),
      /*default_path=*/
      GetDefaultFileToExport(suggested_path, handler_->GetCaseId(),
                             handler_->GetDataCollectionTimestamp()),
      /*file_types=*/nullptr,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(), owning_window,
      /*params=*/nullptr);
}

void SupportToolMessageHandler::FileSelected(const base::FilePath& path,
                                             int index,
                                             void* params) {
  FireWebUIListener("support-data-export-started");
  select_file_dialog_.reset();
  this->handler_->ExportCollectedData(
      std::move(selected_pii_to_keep_), path,
      base::BindOnce(&SupportToolMessageHandler::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SupportToolMessageHandler::FileSelectionCanceled(void* params) {
  selected_pii_to_keep_.clear();
  select_file_dialog_.reset();
}

// Checks `errors` and fires WebUIListener with the error message or the
// exported path according to the returned errors.
// type DataExportResult = {
//  success: boolean,
//  path: string,
//  error: string,
// }
void SupportToolMessageHandler::OnDataExportDone(
    base::FilePath path,
    std::set<SupportToolError> errors) {
  data_path_ = path;
  base::Value::Dict data_export_result;
  const auto& export_error = std::find_if(
      errors.begin(), errors.end(), [](const SupportToolError& error) {
        return (error.error_code == SupportToolErrorCode::kDataExportError);
      });
  if (export_error == errors.end()) {
    data_export_result.Set("success", true);
    data_export_result.Set("path", path.BaseName().AsUTF8Unsafe());
    data_export_result.Set("error", std::string());
  } else {
    // If a data export error is found in the returned set of errors, send the
    // error message to UI with empty string as path since it means the export
    // operation has failed.
    data_export_result.Set("success", false);
    data_export_result.Set("path", std::string());
    data_export_result.Set("error", export_error->error_message);
  }
  FireWebUIListener("data-export-completed", data_export_result);
}

void SupportToolMessageHandler::HandleShowExportedDataInFolder(
    const base::Value::List& args) {
  platform_util::ShowItemInFolder(Profile::FromWebUI(web_ui()), data_path_);
}

void SupportToolMessageHandler::HandleGenerateCustomizedURL(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  const base::Value& callback_id = args[0];
  std::string case_id = args[1].GetString();
  const base::Value::List* data_collectors = args[2].GetIfList();
  DCHECK(data_collectors);
  ResolveJavascriptCallback(callback_id,
                            GenerateCustomizedURL(case_id, data_collectors));
}

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolUI
//
////////////////////////////////////////////////////////////////////////////////

SupportToolUI::SupportToolUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SupportToolMessageHandler>());

  // Set up the chrome://support-tool/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(
      profile, CreateSupportToolHTMLSource(web_ui->GetWebContents()->GetURL()));
}

SupportToolUI::~SupportToolUI() = default;

bool SupportToolUI::IsEnabled(Profile* profile) {
  return webui::IsEnterpriseManaged() || !profile->IsGuestSession();
}
