// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill_assistant/password_change/apc_client.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/move_password_to_account_store_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/driver/sync_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils_chromeos.h"
#endif

namespace {

using password_manager::CredentialUIEntry;

// The error message returned to the UI when Chrome refuses to start multiple
// exports.
const char kExportInProgress[] = "in-progress";
// The error message returned to the UI when the user fails to reauthenticate.
const char kReauthenticationFailed[] = "reauth-failed";

// Map password_manager::ExportProgressStatus to
// extensions::api::passwords_private::ExportProgressStatus.
extensions::api::passwords_private::ExportProgressStatus ConvertStatus(
    password_manager::ExportProgressStatus status) {
  switch (status) {
    case password_manager::ExportProgressStatus::NOT_STARTED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_NOT_STARTED;
    case password_manager::ExportProgressStatus::IN_PROGRESS:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_IN_PROGRESS;
    case password_manager::ExportProgressStatus::SUCCEEDED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_SUCCEEDED;
    case password_manager::ExportProgressStatus::FAILED_CANCELLED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_FAILED_CANCELLED;
    case password_manager::ExportProgressStatus::FAILED_WRITE_FAILED:
      return extensions::api::passwords_private::ExportProgressStatus::
          EXPORT_PROGRESS_STATUS_FAILED_WRITE_FAILED;
  }

  NOTREACHED();
  return extensions::api::passwords_private::ExportProgressStatus::
      EXPORT_PROGRESS_STATUS_NONE;
}

password_manager::ReauthPurpose GetReauthPurpose(
    extensions::api::passwords_private::PlaintextReason reason) {
  switch (reason) {
    case extensions::api::passwords_private::PLAINTEXT_REASON_VIEW:
      return password_manager::ReauthPurpose::VIEW_PASSWORD;
    case extensions::api::passwords_private::PLAINTEXT_REASON_COPY:
      return password_manager::ReauthPurpose::COPY_PASSWORD;
    case extensions::api::passwords_private::PLAINTEXT_REASON_EDIT:
      return password_manager::ReauthPurpose::EDIT_PASSWORD;
    case extensions::api::passwords_private::PLAINTEXT_REASON_NONE:
      break;
  }

  NOTREACHED();
  return password_manager::ReauthPurpose::VIEW_PASSWORD;
}

password_manager::metrics_util::AccessPasswordInSettingsEvent
ConvertPlaintextReason(
    extensions::api::passwords_private::PlaintextReason reason) {
  switch (reason) {
    case extensions::api::passwords_private::PLAINTEXT_REASON_COPY:
      return password_manager::metrics_util::ACCESS_PASSWORD_COPIED;
    case extensions::api::passwords_private::PLAINTEXT_REASON_VIEW:
      return password_manager::metrics_util::ACCESS_PASSWORD_VIEWED;
    case extensions::api::passwords_private::PLAINTEXT_REASON_EDIT:
      return password_manager::metrics_util::ACCESS_PASSWORD_EDITED;
    case extensions::api::passwords_private::PLAINTEXT_REASON_NONE:
      NOTREACHED();
      return password_manager::metrics_util::ACCESS_PASSWORD_VIEWED;
  }
}

base::flat_set<password_manager::PasswordForm::Store>
ConvertToPasswordFormStores(
    extensions::api::passwords_private::PasswordStoreSet store) {
  switch (store) {
    case extensions::api::passwords_private::
        PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT:
      return {password_manager::PasswordForm::Store::kProfileStore,
              password_manager::PasswordForm::Store::kAccountStore};
    case extensions::api::passwords_private::PASSWORD_STORE_SET_DEVICE:
      return {password_manager::PasswordForm::Store::kProfileStore};
    case extensions::api::passwords_private::PASSWORD_STORE_SET_ACCOUNT:
      return {password_manager::PasswordForm::Store::kAccountStore};
    default:
      break;
  }
  NOTREACHED();
  return {};
}

extensions::api::passwords_private::PasswordUiEntry
CreatePasswordUiEntryFromCredentialUiEntry(
    int id,
    const CredentialUIEntry& credential) {
  extensions::api::passwords_private::PasswordUiEntry entry;
  entry.urls = extensions::CreateUrlCollectionFromCredential(credential);
  entry.username = base::UTF16ToUTF8(credential.username);
  // TODO(crbug.com/1345899): Fill the note field after authentication in
  // OnRequestCredentialDetailsAuthResult
  entry.note =
      std::make_unique<std::string>(base::UTF16ToUTF8(credential.note.value));
  entry.id = id;
  entry.stored_in = extensions::StoreSetFromCredential(credential);
  entry.is_android_credential =
      password_manager::IsValidAndroidFacetURI(credential.signon_realm);
  if (!credential.federation_origin.opaque()) {
    std::u16string formatted_origin =
        url_formatter::FormatOriginForSecurityDisplay(
            credential.federation_origin,
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

    entry.federation_text =
        std::make_unique<std::string>(l10n_util::GetStringFUTF8(
            IDS_PASSWORDS_VIA_FEDERATION, formatted_origin));
  }
  return entry;
}

extensions::api::passwords_private::ImportEntry ConvertImportEntry(
    const password_manager::ImportEntry& entry) {
  extensions::api::passwords_private::ImportEntry result;
  result.status =
      static_cast<extensions::api::passwords_private::ImportEntryStatus>(
          entry.status);
  result.url = entry.url;
  result.username = entry.username;
  return result;
}

// Maps password_manager::ImportResults to
// extensions::api::passwords_private::ImportResults.
extensions::api::passwords_private::ImportResults ConvertImportResults(
    const password_manager::ImportResults& results) {
  extensions::api::passwords_private::ImportResults private_results;
  private_results.status =
      static_cast<extensions::api::passwords_private::ImportResultsStatus>(
          results.status);
  private_results.number_imported = results.number_imported;
  private_results.file_name = results.file_name;
  private_results.failed_imports.reserve(results.failed_imports.size());
  for (const auto& entry : results.failed_imports)
    private_results.failed_imports.emplace_back(ConvertImportEntry(entry));
  return private_results;
}

}  // namespace

namespace extensions {

PasswordsPrivateDelegateImpl::PasswordsPrivateDelegateImpl(Profile* profile)
    : profile_(profile),
      saved_passwords_presenter_(PasswordStoreFactory::GetForProfile(
                                     profile,
                                     ServiceAccessType::EXPLICIT_ACCESS),
                                 AccountPasswordStoreFactory::GetForProfile(
                                     profile,
                                     ServiceAccessType::EXPLICIT_ACCESS)),
      password_manager_porter_(std::make_unique<PasswordManagerPorter>(
          profile,
          &saved_passwords_presenter_,
          base::BindRepeating(
              &PasswordsPrivateDelegateImpl::OnPasswordsExportProgress,
              base::Unretained(this)))),
      password_access_authenticator_(
          base::BindRepeating(&PasswordsPrivateDelegateImpl::OsReauthCall,
                              base::Unretained(this)),
          base::BindRepeating(
              &PasswordsPrivateDelegateImpl::OsReauthTimeoutCall,
              base::Unretained(this))),
      password_account_storage_settings_watcher_(
          std::make_unique<
              password_manager::PasswordAccountStorageSettingsWatcher>(
              profile_->GetPrefs(),
              SyncServiceFactory::GetForProfile(profile_),
              base::BindRepeating(&PasswordsPrivateDelegateImpl::
                                      OnAccountStorageOptInStateChanged,
                                  base::Unretained(this)))),
      password_check_delegate_(profile,
                               &saved_passwords_presenter_,
                               &credential_id_generator_),
      current_entries_initialized_(false),
      is_initialized_(false),
      web_contents_(nullptr) {
  saved_passwords_presenter_.AddObserver(this);
  saved_passwords_presenter_.Init();
}

PasswordsPrivateDelegateImpl::~PasswordsPrivateDelegateImpl() {
  saved_passwords_presenter_.RemoveObserver(this);
}

void PasswordsPrivateDelegateImpl::GetSavedPasswordsList(
    UiEntriesCallback callback) {
  if (current_entries_initialized_)
    std::move(callback).Run(current_entries_);
  else
    get_saved_passwords_list_callbacks_.push_back(std::move(callback));
}

void PasswordsPrivateDelegateImpl::GetPasswordExceptionsList(
    ExceptionEntriesCallback callback) {
  if (current_entries_initialized_)
    std::move(callback).Run(current_exceptions_);
  else
    get_password_exception_list_callbacks_.push_back(std::move(callback));
}

absl::optional<api::passwords_private::UrlCollection>
PasswordsPrivateDelegateImpl::GetUrlCollection(const std::string& url) {
  GURL url_with_scheme = password_manager_util::ConstructGURLWithScheme(url);
  if (!password_manager_util::IsValidPasswordURL(url_with_scheme)) {
    return absl::nullopt;
  }
  return absl::optional<api::passwords_private::UrlCollection>(
      CreateUrlCollectionFromGURL(
          password_manager_util::StripAuthAndParams(url_with_scheme)));
}

bool PasswordsPrivateDelegateImpl::IsAccountStoreDefault(
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  DCHECK(client->GetPasswordFeatureManager()->IsOptedInForAccountStorage());
  return client->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
         password_manager::PasswordForm::Store::kAccountStore;
}

bool PasswordsPrivateDelegateImpl::AddPassword(
    const std::string& url,
    const std::u16string& username,
    const std::u16string& password,
    const std::u16string& note,
    bool use_account_store,
    content::WebContents* web_contents) {
  password_manager::PasswordForm::Store store_to_use =
      use_account_store ? password_manager::PasswordForm::Store::kAccountStore
                        : password_manager::PasswordForm::Store::kProfileStore;
  CredentialUIEntry credential;
  credential.url = password_manager_util::StripAuthAndParams(
      password_manager_util::ConstructGURLWithScheme(url));
  credential.signon_realm = password_manager::GetSignonRealm(credential.url);
  credential.username = username;
  credential.password = password;
  credential.note = password_manager::PasswordNote(
      /*value=*/note, /*date_created=*/base::Time::Now());
  credential.stored_in = {store_to_use};
  bool success = saved_passwords_presenter_.AddCredential(credential);

  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  // Update the default store to the last used one.
  if (success &&
      client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    client->GetPasswordFeatureManager()->SetDefaultPasswordStore(store_to_use);
  }
  return success;
}

absl::optional<int> PasswordsPrivateDelegateImpl::ChangeSavedPassword(
    int id,
    const api::passwords_private::ChangeSavedPasswordParams& params) {
  const CredentialUIEntry* original_credential =
      credential_id_generator_.TryGetKey(id);
  if (!original_credential)
    return absl::nullopt;

  CredentialUIEntry updated_credential = *original_credential;
  updated_credential.username = base::UTF8ToUTF16(params.username);
  updated_credential.password = base::UTF8ToUTF16(params.password);
  if (params.note) {
    updated_credential.note = password_manager::PasswordNote(
        base::UTF8ToUTF16(*params.note), base::Time::Now());
  }
  switch (saved_passwords_presenter_.EditSavedCredentials(*original_credential,
                                                          updated_credential)) {
    case password_manager::SavedPasswordsPresenter::EditResult::kSuccess:
    case password_manager::SavedPasswordsPresenter::EditResult::kNothingChanged:
      break;
    case password_manager::SavedPasswordsPresenter::EditResult::kNotFound:
    case password_manager::SavedPasswordsPresenter::EditResult::kAlreadyExisits:
    case password_manager::SavedPasswordsPresenter::EditResult::kEmptyPassword:
      return absl::nullopt;
  }

  return credential_id_generator_.GenerateId(updated_credential);
}

void PasswordsPrivateDelegateImpl::RemoveSavedPassword(
    int id,
    api::passwords_private::PasswordStoreSet from_stores) {
  ExecuteFunction(
      base::BindOnce(&PasswordsPrivateDelegateImpl::RemoveEntryInternal,
                     base::Unretained(this), id, from_stores));
}

void PasswordsPrivateDelegateImpl::RemoveEntryInternal(
    int id,
    api::passwords_private::PasswordStoreSet from_stores) {
  const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
  if (!entry) {
    return;
  }

  CredentialUIEntry copy = *entry;
  copy.stored_in = ConvertToPasswordFormStores(from_stores);

  saved_passwords_presenter_.RemoveCredential(copy);

  if (entry->blocked_by_user) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasswordException"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
  }
}

void PasswordsPrivateDelegateImpl::RemovePasswordException(int id) {
  ExecuteFunction(base::BindOnce(
      &PasswordsPrivateDelegateImpl::RemoveEntryInternal,
      base::Unretained(this), id,
      api::passwords_private::PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT));
}

void PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrException() {
  ExecuteFunction(base::BindOnce(
      &PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrExceptionInternal,
      base::Unretained(this)));
}

void PasswordsPrivateDelegateImpl::
    UndoRemoveSavedPasswordOrExceptionInternal() {
  saved_passwords_presenter_.UndoLastRemoval();
}

void PasswordsPrivateDelegateImpl::RequestPlaintextPassword(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    content::WebContents* web_contents) {
  // Save |web_contents| so that it can be used later when OsReauthCall() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method.
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;
  password_access_authenticator_.EnsureUserIsAuthenticated(
      GetReauthPurpose(reason),
      base::BindOnce(
          &PasswordsPrivateDelegateImpl::OnRequestPlaintextPasswordAuthResult,
          weak_ptr_factory_.GetWeakPtr(), id, reason, std::move(callback)));
}

void PasswordsPrivateDelegateImpl::RequestCredentialDetails(
    int id,
    RequestCredentialDetailsCallback callback,
    content::WebContents* web_contents) {
  // Save |web_contents| so that it can be used later when OsReauthCall() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method.
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;
  password_access_authenticator_.EnsureUserIsAuthenticated(
      GetReauthPurpose(api::passwords_private::PLAINTEXT_REASON_VIEW),
      base::BindOnce(
          &PasswordsPrivateDelegateImpl::OnRequestCredentialDetailsAuthResult,
          weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void PasswordsPrivateDelegateImpl::OsReauthCall(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
#if BUILDFLAG(IS_WIN)
  DCHECK(web_contents_);
  bool result = password_manager_util_win::AuthenticateUser(
      web_contents_->GetTopLevelNativeWindow(), purpose);
  std::move(callback).Run(result);
#elif BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kBiometricAuthenticationInSettings)) {
    scoped_refptr<device_reauth::BiometricAuthenticator>
        biometric_authenticator =
            ChromeBiometricAuthenticatorFactory::GetInstance()
                ->GetOrCreateBiometricAuthenticator();
    base::OnceCallback<void()> on_reauth_completed =
        base::BindOnce(&PasswordsPrivateDelegateImpl::OnReauthCompleted,
                       weak_ptr_factory_.GetWeakPtr());

    biometric_authenticator->AuthenticateWithMessage(
        device_reauth::BiometricAuthRequester::kPasswordsInSettings,
        password_manager_util_mac::GetMessageForBiometricLoginPrompt(purpose),
        std::move(callback).Then(std::move(on_reauth_completed)));

    // If AuthenticateWithMessage is called again(UI isn't blocked so user might
    // click multiple times on the button), it invalidates the old request which
    // triggers PasswordsPrivateDelegateImpl::OnReauthCompleted which resets
    // biometric_authenticator_. Having a local variable solves that problem as
    // there's a second scoped_refptr for the authenticator object.
    biometric_authenticator_ = std::move(biometric_authenticator);
  } else {
    bool result = password_manager_util_mac::AuthenticateUser(purpose);
    std::move(callback).Run(result);
  }
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  bool result =
      IsOsReauthAllowedAsh(profile_, GetAuthTokenLifetimeForPurpose(purpose));
  std::move(callback).Run(result);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  IsOsReauthAllowedLacrosAsync(purpose, std::move(callback));
#else
  std::move(callback).Run(true);
#endif
}

void PasswordsPrivateDelegateImpl::OsReauthTimeoutCall() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordManagerAuthTimeout();
}

void PasswordsPrivateDelegateImpl::SetCredentials(
    const std::vector<CredentialUIEntry>& credentials) {
  // Create lists of PasswordUiEntry and ExceptionEntry objects to send to
  // observers.
  current_entries_.clear();
  current_exceptions_.clear();

  for (const CredentialUIEntry& credential : credentials) {
    int id = credential_id_generator_.GenerateId(credential);
    if (credential.blocked_by_user) {
      api::passwords_private::ExceptionEntry current_exception_entry;
      current_exception_entry.urls =
          CreateUrlCollectionFromCredential(credential);
      current_exception_entry.id = id;
      current_exceptions_.push_back(std::move(current_exception_entry));
    } else {
      current_entries_.push_back(
          CreatePasswordUiEntryFromCredentialUiEntry(id, credential));
    }
  }

  if (current_entries_initialized_) {
    DCHECK(get_saved_passwords_list_callbacks_.empty());
    DCHECK(get_password_exception_list_callbacks_.empty());
  }

  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnSavedPasswordsListChanged(current_entries_);
    router->OnPasswordExceptionsListChanged(current_exceptions_);
  }

  current_entries_initialized_ = true;
  InitializeIfNecessary();

  for (auto& callback : get_saved_passwords_list_callbacks_)
    std::move(callback).Run(current_entries_);
  get_saved_passwords_list_callbacks_.clear();
  for (auto& callback : get_password_exception_list_callbacks_)
    std::move(callback).Run(current_exceptions_);
  get_password_exception_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::MovePasswordsToAccount(
    const std::vector<int>& ids,
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);

  if (!client->GetPasswordFeatureManager()->IsOptedInForAccountStorage() ||
      SyncServiceFactory::GetForProfile(profile_)->IsSyncFeatureEnabled()) {
    return;
  }

  std::vector<password_manager::PasswordForm> forms_to_move;
  for (int id : ids) {
    const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
    if (!entry) {
      continue;
    }

    std::vector<password_manager::PasswordForm> corresponding_forms =
        saved_passwords_presenter_.GetCorrespondingPasswordForms(*entry);
    if (corresponding_forms.empty()) {
      continue;
    }

    // password_manager::MovePasswordsToAccountStore() takes care of moving the
    // entire equivalence class, so passing the first element is fine.
    forms_to_move.push_back(std::move(corresponding_forms[0]));
  }

  password_manager::MovePasswordsToAccountStore(
      forms_to_move, client,
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredInSettings);
}

void PasswordsPrivateDelegateImpl::ImportPasswords(
    api::passwords_private::PasswordStoreSet to_store,
    ImportResultsCallback results_callback,
    content::WebContents* web_contents) {
  DCHECK_NE(api::passwords_private::PasswordStoreSet::
                PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT,
            to_store);
  password_manager_porter_->Import(
      web_contents, *ConvertToPasswordFormStores(to_store).begin(),
      base::BindOnce(&ConvertImportResults).Then(std::move(results_callback)));
}

void PasswordsPrivateDelegateImpl::ExportPasswords(
    base::OnceCallback<void(const std::string&)> accepted_callback,
    content::WebContents* web_contents) {
  // Save |web_contents| so that it can be used later when OsReauthCall() is
  // called. Note: This is safe because the |web_contents| is used before
  // exiting this method.
  // TODO(crbug.com/495290): Pass the native window directly to the
  // reauth-handling code.
  web_contents_ = web_contents;
  password_access_authenticator_.ForceUserReauthentication(
      password_manager::ReauthPurpose::EXPORT,
      base::BindOnce(&PasswordsPrivateDelegateImpl::OnExportPasswordsAuthResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(accepted_callback), web_contents));
}

void PasswordsPrivateDelegateImpl::CancelExportPasswords() {
  password_manager_porter_->CancelExport();
}

api::passwords_private::ExportProgressStatus
PasswordsPrivateDelegateImpl::GetExportProgressStatus() {
  return ConvertStatus(password_manager_porter_->GetExportProgressStatus());
}

bool PasswordsPrivateDelegateImpl::IsOptedInForAccountStorage() {
  return password_manager::features_util::IsOptedInForAccountStorage(
      profile_->GetPrefs(), SyncServiceFactory::GetForProfile(profile_));
}

void PasswordsPrivateDelegateImpl::SetAccountStorageOptIn(
    bool opt_in,
    content::WebContents* web_contents) {
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  if (opt_in ==
      client->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    return;
  }
  if (!opt_in) {
    client->GetPasswordFeatureManager()
        ->OptOutOfAccountStorageAndClearSettings();
    return;
  }
  // The opt in pref is automatically set upon successful reauth.
  client->TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint::kPasswordSettings, base::DoNothing());
}

std::vector<api::passwords_private::PasswordUiEntry>
PasswordsPrivateDelegateImpl::GetCompromisedCredentials() {
  return password_check_delegate_.GetCompromisedCredentials();
}

std::vector<api::passwords_private::PasswordUiEntry>
PasswordsPrivateDelegateImpl::GetWeakCredentials() {
  return password_check_delegate_.GetWeakCredentials();
}

bool PasswordsPrivateDelegateImpl::MuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  return password_check_delegate_.MuteInsecureCredential(credential);
}

bool PasswordsPrivateDelegateImpl::UnmuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  return password_check_delegate_.UnmuteInsecureCredential(credential);
}

void PasswordsPrivateDelegateImpl::RecordChangePasswordFlowStarted(
    const api::passwords_private::PasswordUiEntry& credential,
    bool is_manual_flow) {
  password_check_delegate_.RecordChangePasswordFlowStarted(credential,
                                                           is_manual_flow);
}

void PasswordsPrivateDelegateImpl::RefreshScriptsIfNecessary(
    RefreshScriptsIfNecessaryCallback callback) {
  password_check_delegate_.RefreshScriptsIfNecessary(std::move(callback));
}

void PasswordsPrivateDelegateImpl::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  password_check_delegate_.StartPasswordCheck(std::move(callback));
}

void PasswordsPrivateDelegateImpl::StopPasswordCheck() {
  password_check_delegate_.StopPasswordCheck();
}

api::passwords_private::PasswordCheckStatus
PasswordsPrivateDelegateImpl::GetPasswordCheckStatus() {
  return password_check_delegate_.GetPasswordCheckStatus();
}

void PasswordsPrivateDelegateImpl::StartAutomatedPasswordChange(
    const api::passwords_private::PasswordUiEntry& credential,
    StartAutomatedPasswordChangeCallback callback) {
  if (!credential.change_password_url) {
    std::move(callback).Run(false);
    return;
  }

  GURL url =
      url::SchemeHostPort(GURL(*credential.change_password_url)).GetURL();
  if (!url.is_valid()) {
    std::move(callback).Run(false);
    return;
  }

  NavigateParams params(profile_, url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&params);

  if (!navigation_handle) {
    std::move(callback).Run(false);
    return;
  }

  ApcClient* apc_client = ApcClient::GetOrCreateForWebContents(
      navigation_handle.get()->GetWebContents());
  apc_client->Start(url, credential.username,
                    /*skip_login=*/false, std::move(callback));
}

password_manager::InsecureCredentialsManager*
PasswordsPrivateDelegateImpl::GetInsecureCredentialsManager() {
  return password_check_delegate_.GetInsecureCredentialsManager();
}

void PasswordsPrivateDelegateImpl::OnPasswordsExportProgress(
    password_manager::ExportProgressStatus status,
    const std::string& folder_name) {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPasswordsExportProgress(ConvertStatus(status), folder_name);
  }
}

void PasswordsPrivateDelegateImpl::OnRequestPlaintextPasswordAuthResult(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    bool authenticated) {
  if (!authenticated) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  const CredentialUIEntry* entry = credential_id_generator_.TryGetKey(id);
  if (!entry) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (reason == api::passwords_private::PLAINTEXT_REASON_COPY) {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(entry->password);
    clipboard_writer.MarkAsConfidential();
    // In case of copy we don't need to give password back to UI. callback
    // will receive either empty string in case of success or null otherwise.
    // Copying occurs here so javascript doesn't need plaintext password.
    std::move(callback).Run(std::u16string());
  } else {
    std::move(callback).Run(entry->password);
  }
  EmitHistogramsForCredentialAccess(*entry, reason);
}

void PasswordsPrivateDelegateImpl::OnRequestCredentialDetailsAuthResult(
    int id,
    RequestCredentialDetailsCallback callback,
    bool authenticated) {
  if (!authenticated) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  const CredentialUIEntry* credential = credential_id_generator_.TryGetKey(id);
  if (!credential) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  api::passwords_private::PasswordUiEntry password_ui_entry =
      CreatePasswordUiEntryFromCredentialUiEntry(id, *credential);
  password_ui_entry.password =
      std::make_unique<std::string>(base::UTF16ToUTF8(credential->password));
  std::move(callback).Run(std::move(password_ui_entry));

  EmitHistogramsForCredentialAccess(
      *credential, api::passwords_private::PLAINTEXT_REASON_VIEW);
}

void PasswordsPrivateDelegateImpl::OnExportPasswordsAuthResult(
    base::OnceCallback<void(const std::string&)> accepted_callback,
    content::WebContents* web_contents,
    bool authenticated) {
  if (!authenticated) {
    std::move(accepted_callback).Run(kReauthenticationFailed);
    return;
  }

  bool accepted = password_manager_porter_->Export(web_contents);
  std::move(accepted_callback)
      .Run(accepted ? std::string() : kExportInProgress);
}

void PasswordsPrivateDelegateImpl::OnAccountStorageOptInStateChanged() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnAccountStorageOptInStateChanged(IsOptedInForAccountStorage());
  }
}

void PasswordsPrivateDelegateImpl::Shutdown() {
  password_account_storage_settings_watcher_.reset();
  password_manager_porter_.reset();
  biometric_authenticator_.reset();
}

void PasswordsPrivateDelegateImpl::OnReauthCompleted() {
  biometric_authenticator_.reset();
}

void PasswordsPrivateDelegateImpl::ExecuteFunction(base::OnceClosure callback) {
  if (is_initialized_) {
    std::move(callback).Run();
    return;
  }

  pre_initialization_callbacks_.emplace_back(std::move(callback));
}

void PasswordsPrivateDelegateImpl::OnSavedPasswordsChanged(
    password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords) {
  SetCredentials(saved_passwords_presenter_.GetSavedCredentials());
}

void PasswordsPrivateDelegateImpl::InitializeIfNecessary() {
  if (is_initialized_ || !current_entries_initialized_)
    return;

  is_initialized_ = true;

  for (base::OnceClosure& callback : pre_initialization_callbacks_)
    std::move(callback).Run();
  pre_initialization_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::EmitHistogramsForCredentialAccess(
    const CredentialUIEntry& entry,
    api::passwords_private::PlaintextReason reason) {
  syncer::SyncService* sync_service = nullptr;
  if (SyncServiceFactory::HasSyncService(profile_)) {
    sync_service = SyncServiceFactory::GetForProfile(profile_);
  }
  if (password_manager::sync_util::IsSyncAccountCredential(
          entry.url, entry.username, sync_service,
          IdentityManagerFactory::GetForProfile(profile_))) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccessPasswordInSettings",
      ConvertPlaintextReason(reason),
      password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
}

}  // namespace extensions
