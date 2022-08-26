// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_piece_forward.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

class PasswordUndoHelper;

// This interface provides a way for clients to obtain a list of all saved
// passwords and register themselves as observers for changes. In contrast to
// simply registering oneself as an observer of a password store directly, this
// class possibly responds to changes in multiple password stores, such as the
// local and account store used for passwords for account store users.
// Furthermore, this class exposes a direct mean to edit a password, and
// notifies its observers about this event. An example use case for this is the
// bulk check settings page, where an edit operation in that page should result
// in the new password to be checked, whereas other password edit operations
// (such as visiting a change password form and then updating the password in
// Chrome) should not trigger a check.
class SavedPasswordsPresenter : public PasswordStoreInterface::Observer,
                                public PasswordStoreConsumer {
 public:
  using SavedPasswordsView = base::span<const PasswordForm>;

  // Observer interface. Clients can implement this to get notified about
  // changes to the list of saved passwords or if a given password was edited
  // Clients can register and de-register themselves, and are expected to do so
  // before the presenter gets out of scope.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer when a password is edited or the list of saved
    // passwords changed.
    //
    // OnEdited() will be invoked synchronously if EditPassword() is invoked
    // with a password that was present in |passwords_|.
    // |password.password_value| will be equal to |new_password| in this case.
    virtual void OnEdited(const PasswordForm& password) {}
    // OnSavedPasswordsChanged() gets invoked asynchronously after a change to
    // the underlying password store happens. This might be due to a call to
    // EditPassword(), but can also happen if passwords are added or removed due
    // to other reasons.
    virtual void OnSavedPasswordsChanged(SavedPasswordsView passwords) {}
  };

  // Result of EditSavedCredentials.
  enum class EditResult {
    // Some credentials were successfully updated.
    kSuccess,
    // New credential matches the old one so nothing was changed.
    kNothingChanged,
    // Credential couldn't be found in the store.
    kNotFound,
    // Credentials with the same username and sign on realm already exists.
    kAlreadyExisits,
    // Password was empty.
    kEmptyPassword,
    kMaxValue = kEmptyPassword,
  };

  // Result of AddCredentialsCallback.
  enum class AddResult {
    // Credential is expected to be added successfully.
    kSuccess,
    // Credential is invalid.
    kInvalid,
    // Credential already exists in the profile store.
    kExistsInProfileStore,
    // Credential already exists in the account store.
    kExistsInAccountStore,
    // Credential already exists in the profile and account store.
    kExistsInProfileAndAccountStore,
    kMaxValue = kExistsInProfileAndAccountStore,
  };

  using AddCredentialsCallback =
      base::OnceCallback<void(const std::vector<AddResult>&)>;

  explicit SavedPasswordsPresenter(
      scoped_refptr<PasswordStoreInterface> profile_store,
      scoped_refptr<PasswordStoreInterface> account_store = nullptr);
  ~SavedPasswordsPresenter() override;

  // Initializes the presenter and makes it issue the first request for all
  // saved passwords.
  void Init();

  // Removes the credential and all its duplicates from the store.
  bool RemoveCredential(const CredentialUIEntry& credential);

  // Cancels the last removal operation.
  void UndoLastRemoval();

  // Adds the |credential| to the specified store. Returns true if the password
  // was added, false if |credential|'s data is not valid (invalid url/empty
  // password), or an entry with such signon_realm and username already exists
  // in any (profile or account) store.
  bool AddCredential(const CredentialUIEntry& credential,
                     password_manager::PasswordForm::Type type =
                         password_manager::PasswordForm::Type::kManuallyAdded);

  // Adds |credentials| to the specified store.
  // Credentials that have invalid data or already exist are ignored.
  //
  // NOTE: Informing observers of credentials belonging to mixed types of stores
  // is not supported.
  //
  // For a single credential the behaviour is identical to AddCredential method.
  //
  // The result is conveyed in AddCredentialsCallback: a vector of corresponding
  // AddResult statuses.
  void AddCredentials(const std::vector<CredentialUIEntry>& credentials,
                      password_manager::PasswordForm::Type type,
                      AddCredentialsCallback completion);

  // Modifies all the saved credentials matching |original_credential| to
  // |updated_credential|. Only username, password, notes and password issues
  // are modifiable.
  EditResult EditSavedCredentials(const CredentialUIEntry& original_credential,
                                  const CredentialUIEntry& updated_credential);

  // Returns a list of the currently saved credentials.
  SavedPasswordsView GetSavedPasswords() const;

  // Returns a list of unique passwords which includes normal credentials,
  // federated credentials and blocked forms. If a same form is present both on
  // account and profile stores it will be represented as a single entity.
  // Uniqueness is determined using site name, username, password. For Android
  // credentials package name is also taken into account and for Federated
  // credentials federation origin.
  std::vector<CredentialUIEntry> GetSavedCredentials() const;

  // Returns PasswordForms corresponding to |credential|.
  std::vector<PasswordForm> GetCorrespondingPasswordForms(
      const CredentialUIEntry& credential) const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Adds the |credential| to the specified store.
  // Expects a credential that is valid to be added - can be verified by
  // calling `GetExpectedAddResult()` before. `completion` callback wil be run
  // after the DB call is completed.
  void AddCredentialAsync(const CredentialUIEntry& credential,
                          password_manager::PasswordForm::Type type,
                          base::OnceClosure completion);

  using DuplicatePasswordsMap = std::multimap<std::string, PasswordForm>;
  // PasswordStoreInterface::Observer
  void OnLoginsChanged(PasswordStoreInterface* store,
                       const PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStoreInterface* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Notify observers about changes in the compromised credentials.
  void NotifyEdited(const PasswordForm& password);
  void NotifySavedPasswordsChanged();

  void RemoveObservers();
  void AddObservers();

  // Returns the expected result for adding |credential|, looks for
  // missing/invalid fields and checks if the credential already exists in the
  // memory cache.
  AddResult GetExpectedAddResult(const CredentialUIEntry& credential) const;

  // Returns the `profile_store_` or `account_store_` if `form` is stored in
  // the profile store or the account store accordingly. This function should
  // be used only for credential stored in a single store.
  PasswordStoreInterface& GetStoreFor(const PasswordForm& form);

  // Try to unblocklist in both stores.If credentials don't
  // exist, the unblocklist operation is a no-op.
  void UnblocklistBothStores(const CredentialUIEntry& credential);

  // The password stores containing the saved passwords.
  scoped_refptr<PasswordStoreInterface> profile_store_;
  scoped_refptr<PasswordStoreInterface> account_store_;

  std::unique_ptr<PasswordUndoHelper> undo_helper_;

  // Cache of the most recently obtained saved passwords.
  std::vector<PasswordForm> passwords_;

  // Structure used to deduplicate list of passwords.
  DuplicatePasswordsMap sort_key_to_password_forms_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<SavedPasswordsPresenter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
