// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_MAC_BIOMETRIC_AUTHENTICATOR_MAC_H_
#define CHROME_BROWSER_DEVICE_REAUTH_MAC_BIOMETRIC_AUTHENTICATOR_MAC_H_

#include "base/callback.h"
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_common.h"
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"
#include "components/device_reauth/biometric_authenticator.h"

namespace device {
namespace fido {
namespace mac {
class TouchIdContext;
}  // namespace mac
}  // namespace fido
}  // namespace device

class BiometricAuthenticatorMac : public ChromeBiometricAuthenticatorCommon {
 public:
  // Returns true, when biometrics are available and also the device screen lock
  // is setup, false otherwise.
  bool CanAuthenticate(
      device_reauth::BiometricAuthRequester requester) override;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Note: this only supports one authentication
  // request at a time.
  // |use_last_valid_auth| if set to false, ignores the grace 60 seconds
  // period between the last valid authentication and the current
  // authentication, and re-invokes system authentication.
  void Authenticate(device_reauth::BiometricAuthRequester requester,
                    AuthenticateCallback callback,
                    bool use_last_valid_auth) override;

  // Trigges an authentication flow based on biometrics.
  // Creates touchIdAuthentication object, request user to authenticate(proper
  // box with that information will appear on the screen and the `message` will
  // be displayed there) using his touchId or if it's not setUp default one with
  // password will appear.
  void AuthenticateWithMessage(device_reauth::BiometricAuthRequester requester,
                               const std::u16string message,
                               AuthenticateCallback callback) override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel(device_reauth::BiometricAuthRequester requester) override;

 private:
  friend class ChromeBiometricAuthenticatorFactory;
  BiometricAuthenticatorMac();
  ~BiometricAuthenticatorMac() override;

  // Called when the authentication compeletes with the result.
  void OnAuthenticationCompleted(bool result);

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // TouchId authenticator object that will handle biometric authentication
  // itself.
  std::unique_ptr<device::fido::mac::TouchIdContext> touch_id_auth_context_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<BiometricAuthenticatorMac> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_MAC_BIOMETRIC_AUTHENTICATOR_MAC_H_
