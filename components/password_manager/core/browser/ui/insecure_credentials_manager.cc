// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/password_manager/core/browser/ui/weak_check_utility.h"
#endif

namespace password_manager {

namespace {

bool SupportsMuteOperation(InsecureType insecure_type) {
  return (insecure_type == InsecureType::kLeaked ||
          insecure_type == InsecureType::kPhished);
}

// The function is only used by the weak check.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
base::flat_set<std::u16string> ExtractPasswords(
    SavedPasswordsPresenter::SavedPasswordsView password_forms) {
  return base::MakeFlatSet<std::u16string>(password_forms, {},
                                           &PasswordForm::password_value);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

InsecureCredentialsManager::InsecureCredentialsManager(
    SavedPasswordsPresenter* presenter,
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store)
    : presenter_(presenter),
      profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)) {
  observed_saved_password_presenter_.Observe(presenter_.get());
}

InsecureCredentialsManager::~InsecureCredentialsManager() = default;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void InsecureCredentialsManager::StartWeakCheck(
    base::OnceClosure on_check_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&BulkWeakCheck,
                     ExtractPasswords(presenter_->GetSavedPasswords())),
      base::BindOnce(&InsecureCredentialsManager::OnWeakCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer())
          .Then(std::move(on_check_done)));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void InsecureCredentialsManager::SaveInsecureCredential(
    const LeakCheckCredential& credential) {
  // Iterate over all currently saved credentials and mark those as insecure
  // that have the same canonicalized username and password.
  const std::u16string canonicalized_username =
      CanonicalizeUsername(credential.username());
  for (const PasswordForm& saved_password : presenter_->GetSavedPasswords()) {
    if (saved_password.password_value == credential.password() &&
        CanonicalizeUsername(saved_password.username_value) ==
            canonicalized_username &&
        !saved_password.password_issues.contains(InsecureType::kLeaked)) {
      PasswordForm form_to_update = saved_password;
      form_to_update.password_issues.insert_or_assign(
          InsecureType::kLeaked,
          InsecurityMetadata(base::Time::Now(), IsMuted(false)));
      GetStoreFor(saved_password).UpdateLogin(form_to_update);
    }
  }
}

bool InsecureCredentialsManager::MuteCredential(
    const CredentialUIEntry& credential) {
  CredentialUIEntry updated_credential = credential;
  for (auto& password_issue : updated_credential.password_issues) {
    if (!password_issue.second.is_muted.value() &&
        SupportsMuteOperation(password_issue.first)) {
      password_issue.second.is_muted = IsMuted(true);
    }
  }
  return presenter_->EditSavedCredentials(credential, updated_credential) ==
         SavedPasswordsPresenter::EditResult::kSuccess;
}

bool InsecureCredentialsManager::UnmuteCredential(
    const CredentialUIEntry& credential) {
  CredentialUIEntry updated_credential = credential;
  for (auto& password_issue : updated_credential.password_issues) {
    if (password_issue.second.is_muted.value() &&
        SupportsMuteOperation(password_issue.first)) {
      password_issue.second.is_muted = IsMuted(false);
    }
  }
  return presenter_->EditSavedCredentials(credential, updated_credential) ==
         SavedPasswordsPresenter::EditResult::kSuccess;
}

std::vector<CredentialUIEntry>
InsecureCredentialsManager::GetInsecureCredentialEntries() const {
  DCHECK(presenter_);
  std::vector<CredentialUIEntry> credentials =
      presenter_->GetSavedCredentials();
  if (base::GetFieldTrialParamByFeatureAsBool(
          password_manager::features::kPasswordChangeInSettings,
          password_manager::features::
              kPasswordChangeInSettingsWithForcedWarningForEverySite,
          /*default_value=*/false)) {
    // If a flag is set to return every credential as compromised, ensure that
    // all credentials contain a "leak" password issue.
    for (auto& credential : credentials) {
      if (!credential.IsLeaked() && !credential.IsPhished()) {
        credential.password_issues[InsecureType::kLeaked] =
            InsecurityMetadata();
      }
    }
  } else {
    // Otherwise erase entries which aren't leaked and phished.
    base::EraseIf(credentials, [](const auto& credential) {
      return !credential.IsLeaked() && !credential.IsPhished();
    });
  }

  return credentials;
}

std::vector<CredentialUIEntry>
InsecureCredentialsManager::GetWeakCredentialEntries() const {
  DCHECK(presenter_);
  std::vector<CredentialUIEntry> credentials =
      presenter_->GetSavedCredentials();
  base::EraseIf(credentials, [this](const auto& credential) {
    return !weak_passwords_.contains(credential.password);
  });
  return credentials;
}

void InsecureCredentialsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InsecureCredentialsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InsecureCredentialsManager::OnWeakCheckDone(
    base::ElapsedTimer timer_since_weak_check_start,
    base::flat_set<std::u16string> weak_passwords) {
  base::UmaHistogramTimes("PasswordManager.WeakCheck.Time",
                          timer_since_weak_check_start.Elapsed());
  weak_passwords_ = std::move(weak_passwords);
  NotifyWeakCredentialsChanged();
}

void InsecureCredentialsManager::OnEdited(const PasswordForm& form) {
  // The WeakCheck is a Desktop only feature for now. Disable on Mobile to avoid
  // pulling in a big dependency on zxcvbn.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  const std::u16string& password = form.password_value;
  if (weak_passwords_.contains(password) || !IsWeak(password)) {
    // Either the password is already known to be weak, or it is not weak at
    // all. In both cases there is nothing to do.
    return;
  }

  weak_passwords_.insert(password);
  NotifyWeakCredentialsChanged();
#endif
}

// Re-computes the list of insecure credentials with passwords after obtaining a
// new list of saved passwords.
void InsecureCredentialsManager::OnSavedPasswordsChanged(
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  NotifyInsecureCredentialsChanged();
  NotifyWeakCredentialsChanged();
}

void InsecureCredentialsManager::NotifyInsecureCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.OnInsecureCredentialsChanged();
  }
}

void InsecureCredentialsManager::NotifyWeakCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.OnWeakCredentialsChanged();
  }
}

PasswordStoreInterface& InsecureCredentialsManager::GetStoreFor(
    const PasswordForm& form) {
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

}  // namespace password_manager
