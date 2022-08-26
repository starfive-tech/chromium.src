// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator.h"

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using base::SysUTF8ToNSString;

namespace {
// Checks for existing credentials with the same url and username.
bool CheckForDuplicates(
    GURL url,
    NSString* username,
    std::vector<password_manager::CredentialUIEntry> credentials) {
  std::string signon_realm = password_manager::GetSignonRealm(
      password_manager_util::StripAuthAndParams(url));
  std::u16string username_value = SysNSStringToUTF16(username);
  auto have_equal_username_and_realm =
      [&signon_realm,
       &username_value](const password_manager::CredentialUIEntry& credential) {
        return signon_realm == credential.signon_realm &&
               username_value == credential.username;
      };
  if (base::ranges::any_of(credentials, have_equal_username_and_realm))
    return true;
  return false;
}
}

@interface AddPasswordMediator () <PasswordDetailsTableViewControllerDelegate> {
  // Password Check manager.
  IOSChromePasswordCheckManager* _manager;
  // Used to create and run validation tasks.
  std::unique_ptr<base::CancelableTaskTracker> _validationTaskTracker;
}

// Caches the password form data submitted by the user. This value is set only
// when the user tries to save a credential which has username and site similar
// to an existing credential.
@property(nonatomic, readonly) absl::optional<password_manager::PasswordForm>
    cachedPasswordForm;

// Delegate for this mediator.
@property(nonatomic, weak) id<AddPasswordMediatorDelegate> delegate;

// Task runner on which validation operations happen.
@property(nonatomic, assign) scoped_refptr<base::SequencedTaskRunner>
    sequencedTaskRunner;

// Stores the url entered in the website field.
@property(nonatomic, assign) GURL URL;

@end

@implementation AddPasswordMediator

- (instancetype)initWithDelegate:(id<AddPasswordMediatorDelegate>)delegate
            passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _manager = manager;
    _sequencedTaskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    _validationTaskTracker = std::make_unique<base::CancelableTaskTracker>();
  }
  return self;
}

- (void)setConsumer:(id<AddPasswordDetailsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
}

- (void)dealloc {
  _validationTaskTracker->TryCancelAll();
  _validationTaskTracker.reset();
}

#pragma mark - PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password {
  NOTREACHED();
}

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
                didAddPasswordDetails:(NSString*)username
                             password:(NSString*)password {
  if (_validationTaskTracker->HasTrackedTasks()) {
    // If the task tracker has pending tasks and the "Save" button is pressed,
    // don't do anything.
    return;
  }

  DCHECK([self isURLValid]);

  password_manager::CredentialUIEntry credential;
  credential.url = self.URL;
  credential.signon_realm = password_manager::GetSignonRealm(credential.url);
  credential.username = SysNSStringToUTF16(username);
  credential.password = SysNSStringToUTF16(password);
  credential.stored_in = {password_manager::PasswordForm::Store::kProfileStore};

  _manager->GetSavedPasswordsPresenter()->AddCredential(credential);
  [self.delegate setUpdatedPassword:credential];
  [self.delegate dismissPasswordDetailsTableViewController];
}

- (void)checkForDuplicates:(NSString*)username {
  _validationTaskTracker->TryCancelAll();
  if (![self isURLValid]) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _validationTaskTracker->PostTaskAndReplyWithResult(
      _sequencedTaskRunner.get(), FROM_HERE,
      base::BindOnce(
          &CheckForDuplicates, self.URL, username,
          _manager->GetSavedPasswordsPresenter()->GetSavedCredentials()),
      base::BindOnce(^(bool duplicateFound) {
        [weakSelf.consumer onDuplicateCheckCompletion:duplicateFound];
      }));
}

- (void)showExistingCredential:(NSString*)username {
  if (![self isURLValid]) {
    return;
  }

  std::string signon_realm = password_manager::GetSignonRealm(
      password_manager_util::StripAuthAndParams(self.URL));
  std::u16string username_value = SysNSStringToUTF16(username);
  for (const auto& credential :
       _manager->GetSavedPasswordsPresenter()->GetSavedCredentials()) {
    if (credential.signon_realm == signon_realm &&
        credential.username == username_value) {
      [self.delegate showPasswordDetailsControllerWithCredential:credential];
      return;
    }
  }
  NOTREACHED();
}

- (void)didCancelAddPasswordDetails {
  [self.delegate dismissPasswordDetailsTableViewController];
}

- (void)setWebsiteURL:(NSString*)website {
  self.URL = password_manager_util::ConstructGURLWithScheme(
      SysNSStringToUTF8(website));
}

- (BOOL)isURLValid {
  return self.URL.is_valid() && self.URL.SchemeIsHTTPOrHTTPS();
}

- (BOOL)isTLDMissing {
  std::string hostname = self.URL.host();
  return hostname.find('.') == std::string::npos;
}

- (BOOL)isUsernameReused:(NSString*)newUsername {
  return NO;
}

@end
