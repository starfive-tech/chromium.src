// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_performer.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/auth_session_status.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

bool IsKioskUserType(user_manager::UserType type) {
  return type == user_manager::USER_TYPE_KIOSK_APP ||
         type == user_manager::USER_TYPE_ARC_KIOSK_APP ||
         type == user_manager::USER_TYPE_WEB_KIOSK_APP;
}

user_data_auth::AuthIntent AuthIntentToProto(AuthSessionIntent intent) {
  switch (intent) {
    case AuthSessionIntent::kDecrypt:
      return user_data_auth::AUTH_INTENT_DECRYPT;
    case AuthSessionIntent::kVerifyOnly:
      return user_data_auth::AUTH_INTENT_VERIFY_ONLY;
  }
}

}  // namespace

AuthPerformer::AuthPerformer(base::raw_ptr<UserDataAuthClient> client)
    : client_(client) {
  DCHECK(client_);
}

AuthPerformer::~AuthPerformer() = default;

void AuthPerformer::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<AuthPerformer> AuthPerformer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AuthPerformer::StartAuthSession(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     AuthSessionIntent intent,
                                     StartSessionCallback callback) {
  client_->WaitForServiceToBeAvailable(base::BindOnce(
      &AuthPerformer::OnServiceRunning, weak_factory_.GetWeakPtr(),
      std::move(context), ephemeral, intent, std::move(callback)));
}

void AuthPerformer::OnServiceRunning(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     AuthSessionIntent intent,
                                     StartSessionCallback callback,
                                     bool service_is_available) {
  if (!service_is_available) {
    // TODO(crbug.com/1262139): Maybe have this error surfaced to UI.
    LOG(FATAL) << "Cryptohome service could not start";
  }
  LOGIN_LOG(EVENT) << "Starting AuthSession";
  user_data_auth::StartAuthSessionRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());
  request.set_intent(AuthIntentToProto(intent));

  if (ephemeral) {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER);
  } else {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_NONE);
  }

  client_->StartAuthSession(
      request, base::BindOnce(&AuthPerformer::OnStartAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::AuthenticateUsingKnowledgeKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(context->GetChallengeResponseKeys().empty());
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    DCHECK(!context->IsUsingPin());
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthPerformer::HashKeyAndAuthenticate, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }  // plain-text password

  auto* key = context->GetKey();
  const auto& auth_factors = context->GetAuthFactorsData();

  // The login code might speculatively set the "gaia" label in the user
  // context, however at the cryptohome level the existing user key's label can
  // be either "gaia" or "legacy-N" - which is what we need to use when talking
  // to cryptohome. If in cryptohome, "gaia" is indeed the label, then at the
  // end of this operation, gaia would be returned. This case applies to only
  // "gaia" labels only because they are created at oobe.
  if (key->GetLabel() == kCryptohomeGaiaKeyLabel || key->GetLabel().empty()) {
    if (features::IsUseAuthFactorsEnabled()) {
      auto* factor = auth_factors.FindOnlinePasswordFactor();
      if (factor == nullptr) {
        LOGIN_LOG(ERROR) << "Could not find Password key";
        std::move(callback).Run(
            std::move(context),
            AuthenticationError{
                user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
        return;
      }
      key->SetLabel(factor->ref().label().value());
    } else {
      const cryptohome::KeyDefinition* key_def =
          auth_factors.FindOnlinePasswordKey();
      if (!key_def) {
        LOGIN_LOG(ERROR) << "Could not find Password key";
        std::move(callback).Run(
            std::move(context),
            AuthenticationError{
                user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
        return;
      }
      key->SetLabel(key_def->label.value());
    }
  }

  if (features::IsUseAuthFactorsEnabled()) {
    LOGIN_LOG(EVENT) << "Authenticating using factor "
                     << context->GetKey()->GetKeyType();
    user_data_auth::AuthenticateAuthFactorRequest request;
    request.set_auth_session_id(context->GetAuthSessionId());

    if (context->IsUsingPin()) {
      cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                    cryptohome::KeyLabel{key->GetLabel()}};
      cryptohome::AuthFactorInput input(
          cryptohome::AuthFactorInput::Pin{key->GetSecret()});
      cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
      request.set_auth_factor_label(ref.label().value());
    } else {
      cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword,
                                    cryptohome::KeyLabel{key->GetLabel()}};
      cryptohome::AuthFactorInput input(
          cryptohome::AuthFactorInput::Password{key->GetSecret()});

      cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
      request.set_auth_factor_label(ref.label().value());
    }
    client_->AuthenticateAuthFactor(
        request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthFactor,
                                weak_factory_.GetWeakPtr(), std::move(context),
                                std::move(callback)));
    return;
  }  // features::IsUseAuthFactorsEnabled()
  LOGIN_LOG(EVENT) << "Authenticating using key "
                   << context->GetKey()->GetKeyType();

  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* cryptohome_key =
      request.mutable_authorization()->mutable_key();
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateKeyDefFromUserContext(*context),
      cryptohome_key);

  client_->AuthenticateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::HashKeyAndAuthenticate(std::unique_ptr<UserContext> context,
                                           AuthOperationCallback callback,
                                           const std::string& system_salt) {
  context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               system_salt);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateUsingChallengeResponseKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetChallengeResponseKeys().empty());
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";
  LOGIN_LOG(EVENT) << "Authenticating using challenge-response";

  if (features::IsUseAuthFactorsEnabled()) {
    NOTIMPLEMENTED()
        << "SmartCard keys are not implemented in AuthFactors yet.";
  } else {
    user_data_auth::AuthenticateAuthSessionRequest request;
    request.set_auth_session_id(context->GetAuthSessionId());

    *request.mutable_authorization() = CreateAuthorizationRequestFromKeyDef(
        cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
            *context));

    client_->AuthenticateAuthSession(
        request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                                weak_factory_.GetWeakPtr(), std::move(context),
                                std::move(callback)));
  }
}

void AuthPerformer::AuthenticateWithPassword(
    const std::string& key_label,
    const std::string& password,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!password.empty()) << "Caller should check for empty password";
  DCHECK(!key_label.empty()) << "Caller should provide correct label";
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  const AuthFactorsData& auth_factors = context->GetAuthFactorsData();
  if (features::IsUseAuthFactorsEnabled()) {
    if (auth_factors.FindPasswordFactor(cryptohome::KeyLabel{key_label}) ==
        nullptr) {
      LOGIN_LOG(ERROR) << "User does not have password factor labeled "
                       << key_label;
      std::move(callback).Run(
          std::move(context),
          AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      return;
    }
  } else {
    if (!auth_factors.HasPasswordKey(key_label)) {
      LOGIN_LOG(ERROR) << "User does not have password factor labeled "
                       << key_label;
      std::move(callback).Run(
          std::move(context),
          AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      return;
    }
  }
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &AuthPerformer::HashPasswordAndAuthenticate, weak_factory_.GetWeakPtr(),
      key_label, password, std::move(context), std::move(callback)));
}

void AuthPerformer::HashPasswordAndAuthenticate(
    const std::string& key_label,
    const std::string& password,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  // Use Key until proper migration to AuthFactors API.
  chromeos::Key password_key(password);
  password_key.SetLabel(key_label);
  password_key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  context->SetKey(password_key);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateWithPin(const std::string& pin,
                                        const std::string& pin_salt,
                                        std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  DCHECK(!pin.empty()) << "Caller should check for empty PIN";
  DCHECK(!pin_salt.empty()) << "Client code should provide correct salt";
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  // Use Key until proper migration to AuthFactors API.
  Key key(pin);

  const auto& auth_factors = context->GetAuthFactorsData();
  if (features::IsUseAuthFactorsEnabled()) {
    auto* factor = auth_factors.FindPinFactor();
    if (factor == nullptr) {
      LOGIN_LOG(ERROR) << "User does not have PIN as factor";
      std::move(callback).Run(
          std::move(context),
          AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      return;
    }
    DCHECK_EQ(factor->ref().label().value(), kCryptohomePinLabel);
    key.SetLabel(factor->ref().label().value());
  } else {
    const cryptohome::KeyDefinition* key_def = auth_factors.FindPinKey();
    if (!key_def) {
      LOGIN_LOG(ERROR) << "User does not have PIN as factor";
      std::move(callback).Run(
          std::move(context),
          AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      return;
    }
    DCHECK_EQ(key_def->label.value(), kCryptohomePinLabel);
    key.SetLabel(key_def->label.value());
  }

  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, pin_salt);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateAsKiosk(std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  LOGIN_LOG(EVENT) << "Authenticating as Kiosk";

  const auto& auth_factors = context->GetAuthFactorsData();

  if (features::IsUseAuthFactorsEnabled()) {
    user_data_auth::AuthenticateAuthFactorRequest request;
    request.set_auth_session_id(context->GetAuthSessionId());
    auto* existing_factor = auth_factors.FindKioskFactor();
    if (existing_factor == nullptr) {
      LOGIN_LOG(ERROR) << "Could not find Kiosk key";
      std::move(callback).Run(
          std::move(context),
          AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      return;
    }
    cryptohome::AuthFactorInput input(cryptohome::AuthFactorInput::Kiosk{});
    cryptohome::SerializeAuthInput(existing_factor->ref(), input,
                                   request.mutable_auth_input());
    request.set_auth_factor_label(existing_factor->ref().label().value());
    client_->AuthenticateAuthFactor(
        request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthFactor,
                                weak_factory_.GetWeakPtr(), std::move(context),
                                std::move(callback)));
    return;
  }  // features::IsUseAuthFactorsEnabled()
  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* key = request.mutable_authorization()->mutable_key();
  cryptohome::KeyData* key_data = key->mutable_data();
  key_data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);

  const cryptohome::KeyDefinition* key_def = auth_factors.FindKioskKey();
  if (!key_def) {
    LOGIN_LOG(ERROR) << "Could not find Kiosk key";
    std::move(callback).Run(
        std::move(context),
        AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }
  key_data->set_label(key_def->label.value());

  client_->AuthenticateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::GetAuthSessionStatus(std::unique_ptr<UserContext> context,
                                         AuthSessionStatusCallback callback) {
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  LOGIN_LOG(EVENT) << "Requesting authsession status";
  user_data_auth::GetAuthSessionStatusRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());

  client_->GetAuthSessionStatus(
      request, base::BindOnce(&AuthPerformer::OnGetAuthSessionStatus,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

/// ---- private callbacks ----

void AuthPerformer::OnStartAuthSession(
    std::unique_ptr<UserContext> context,
    StartSessionCallback callback,
    absl::optional<user_data_auth::StartAuthSessionReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Could not start authsession " << error;
    std::move(callback).Run(false, std::move(context),
                            AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "AuthSession started, user "
                   << (reply->user_exists() ? "exists" : "does not exist");

  context->SetAuthSessionId(reply->auth_session_id());
  if (features::IsUseAuthFactorsEnabled()) {
    std::vector<cryptohome::AuthFactor> next_factors;
    cryptohome::AuthFactorType fallback_type =
        cryptohome::AuthFactorType::kPassword;
    if (IsKioskUserType(context->GetUserType()))
      fallback_type = cryptohome::AuthFactorType::kKiosk;
    for (const auto& factor_proto : reply->auth_factors()) {
      next_factors.emplace_back(
          cryptohome::DeserializeAuthFactor(factor_proto, fallback_type));
    }

    AuthFactorsData auth_factors_data(std::move(next_factors));
    context->SetAuthFactorsData(std::move(auth_factors_data));
  } else {
    // Remember key metadata
    std::vector<cryptohome::KeyDefinition> key_definitions;
    for (const auto& [label, key_data] : reply->key_label_data()) {
      // Backfill key type
      // TODO(crbug.com/1310312): Find if there is any better way.
      cryptohome::KeyData data(key_data);
      if (!data.has_type()) {
        if (IsKioskUserType(context->GetUserType())) {
          LOGIN_LOG(DEBUG) << "Backfilling Kiosk key type for key " << label;
          data.set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
        } else {
          LOGIN_LOG(DEBUG) << "Backfilling Password key type for key " << label;
          data.set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
        }
      }
      // "legacy-0" keys exist as label in map, but might not exist as labels
      // in KeyData.
      if (!data.has_label() || data.label().empty()) {
        data.set_label(label);
      }
      key_definitions.push_back(KeyDataToKeyDefinition(data));
    }
    AuthFactorsData auth_factors_data(std::move(key_definitions));
    context->SetAuthFactorsData(std::move(auth_factors_data));
  }

  std::move(callback).Run(reply->user_exists(), std::move(context),
                          absl::nullopt);
}

void AuthPerformer::OnAuthenticateAuthSession(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply) {
  DCHECK(!features::IsUseAuthFactorsEnabled());
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT) << "Failed to authenticate session, error code " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  DCHECK(reply->authenticated());
  LOGIN_LOG(EVENT) << "Authenticated successfully";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthPerformer::OnAuthenticateAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AuthenticateAuthFactorReply> reply) {
  DCHECK(features::IsUseAuthFactorsEnabled());
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT)
        << "Failed to authenticate session via authfactor, error code "
        << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  DCHECK(reply->authenticated());
  LOGIN_LOG(EVENT) << "Authenticated successfully";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthPerformer::OnGetAuthSessionStatus(
    std::unique_ptr<UserContext> context,
    AuthSessionStatusCallback callback,
    absl::optional<user_data_auth::GetAuthSessionStatusReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);

  if (error == user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN) {
    // Do not trigger error handling
    std::move(callback).Run(AuthSessionStatus(), base::TimeDelta(),
                            std::move(context),
                            /*cryptohome_error=*/absl::nullopt);
    return;
  }

  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT) << "Failed to get authsession status " << error;
    std::move(callback).Run(AuthSessionStatus(), base::TimeDelta(),
                            std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  base::TimeDelta lifetime;
  AuthSessionStatus status;
  switch (reply->status()) {
    case ::user_data_auth::AUTH_SESSION_STATUS_NOT_SET:
    case ::user_data_auth::AUTH_SESSION_STATUS_INVALID_AUTH_SESSION:
      break;
    case ::user_data_auth::AUTH_SESSION_STATUS_FURTHER_FACTOR_REQUIRED:
      status.Put(AuthSessionLevel::kSessionIsValid);
      // Once we support multi-factor authentication (and have partially
      // authenticated sessions) we might need to use value from reply.
      lifetime = base::TimeDelta::Max();
      break;
    case ::user_data_auth::AUTH_SESSION_STATUS_AUTHENTICATED:
      status.Put(AuthSessionLevel::kSessionIsValid);
      status.Put(AuthSessionLevel::kCryptohomeStrong);
      lifetime = base::Seconds(reply->time_left());
      break;
    default:
      NOTREACHED();
  }
  std::move(callback).Run(status, lifetime, std::move(context),
                          /*cryptohome_error=*/absl::nullopt);
}

}  // namespace ash
