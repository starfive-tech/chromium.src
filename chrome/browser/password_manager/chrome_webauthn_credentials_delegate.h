// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace device {
class DiscoverableCredentialMetadata;
}

// Chrome implementation of WebAuthnCredentialsDelegate.
class ChromeWebAuthnCredentialsDelegate
    : public password_manager::WebAuthnCredentialsDelegate {
 public:
  explicit ChromeWebAuthnCredentialsDelegate(
      content::WebContents* web_contents);
  ~ChromeWebAuthnCredentialsDelegate() override;
  ChromeWebAuthnCredentialsDelegate(const ChromeWebAuthnCredentialsDelegate&) =
      delete;
  ChromeWebAuthnCredentialsDelegate operator=(
      const ChromeWebAuthnCredentialsDelegate&) = delete;

  // password_manager::WebAuthnCredentialsDelegate:
  bool IsWebAuthnAutofillEnabled() const override;
  void LaunchWebAuthnFlow() override;
  void SelectWebAuthnCredential(std::string backend_id) override;
  const absl::optional<std::vector<autofill::Suggestion>>&
  GetWebAuthnSuggestions() const override;
  void RetrieveWebAuthnSuggestions(base::OnceClosure callback) override;

  // Method for providing a list of WebAuthn user entities that can be provided
  // as autofill suggestions. This is called when a WebAuthn Conditional UI
  // request is received.
  void OnCredentialsReceived(
      const std::vector<device::DiscoverableCredentialMetadata>& credentials);

 protected:
  const raw_ptr<content::WebContents> web_contents_;

 private:
  // List of autofill suggestions populated from an authenticator from a call
  // to RetrieveWebAuthnSuggestions, and returned to the client via
  // GetWebAuthnSuggestions.
  absl::optional<std::vector<autofill::Suggestion>> suggestions_;

  base::OnceClosure retrieve_suggestions_callback_;

  base::WeakPtrFactory<ChromeWebAuthnCredentialsDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_WEBAUTHN_CREDENTIALS_DELEGATE_H_
