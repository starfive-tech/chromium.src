// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"
#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chromeos/printing/uri.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

namespace {

// Builds a single log entry for device-log.
std::string BuildLogEntry(base::StringPiece method,
                          const GURL& auth_server,
                          const chromeos::Uri& ipp_endpoint = chromeos::Uri(),
                          absl::optional<StatusCode> status = absl::nullopt,
                          const std::string& data = "") {
  std::vector<base::StringPiece> strv;
  strv.reserve(10);
  strv.emplace_back("oauth ");
  strv.emplace_back(method);
  strv.emplace_back(";server=");
  strv.emplace_back(auth_server.possibly_invalid_spec());
  const std::string endpoint = ipp_endpoint.GetNormalized();
  if (!endpoint.empty()) {
    strv.emplace_back(";endpoint=");
    strv.emplace_back(endpoint);
  }
  if (status) {
    strv.emplace_back(";status=");
    strv.emplace_back(ToStringPiece(*status));
  }
  if (!data.empty()) {
    strv.emplace_back(": ");
    strv.emplace_back(data);
  }
  return base::StrCat(strv);
}

// Logs results to device-log and calls `callback` with parameters `status` and
// `data`.
void LogAndCall(StatusCallback callback,
                base::StringPiece method,
                const GURL& auth_server,
                const chromeos::Uri& ipp_endpoint,
                StatusCode status,
                const std::string& data) {
  if (status == StatusCode::kOK || status == StatusCode::kAuthorizationNeeded) {
    PRINTER_LOG(EVENT) << BuildLogEntry(
        method, auth_server, ipp_endpoint, status,
        (status == StatusCode::kOK) ? "" : data);
  } else {
    PRINTER_LOG(ERROR) << BuildLogEntry(method, auth_server, ipp_endpoint,
                                        status, data);
  }
  std::move(callback).Run(status, data);
}

void AddLoggingToCallback(StatusCallback& callback,
                          const base::StringPiece method,
                          const GURL& auth_server,
                          const chromeos::Uri& ipp_endpoint = chromeos::Uri()) {
  // Wrap the `callback` with the function LogAndCall() defined above.
  auto new_call = base::BindOnce(&LogAndCall, std::move(callback), method,
                                 auth_server, ipp_endpoint);
  callback = std::move(new_call);
}

class AuthorizationZonesManagerImpl
    : public AuthorizationZonesManager,
      private ProfileAuthServersSyncBridge::Observer {
 public:
  explicit AuthorizationZonesManagerImpl(Profile* profile)
      : sync_bridge_(ProfileAuthServersSyncBridge::Create(
            this,
            ModelTypeStoreServiceFactory::GetForProfile(profile)
                ->GetStoreFactory())),
        url_loader_factory_(profile->GetURLLoaderFactory()),
        auth_zone_creator_(base::BindRepeating(AuthorizationZone::Create,
                                               url_loader_factory_)) {}

  AuthorizationZonesManagerImpl(
      Profile* profile,
      CreateAuthZoneCallback auth_zone_creator,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory store_factory)
      : sync_bridge_(ProfileAuthServersSyncBridge::CreateForTesting(
            this,
            std::move(change_processor),
            std::move(store_factory))),
        url_loader_factory_(profile->GetURLLoaderFactory()),
        auth_zone_creator_(std::move(auth_zone_creator)) {}

  StatusCode SaveAuthorizationServerAsTrusted(
      const GURL& auth_server) override {
    if (!auth_server.is_valid() || !auth_server.SchemeIs("https") ||
        !auth_server.has_host() || auth_server.has_username() ||
        auth_server.has_query() || auth_server.has_ref()) {
      PRINTER_LOG(USER) << BuildLogEntry(__func__, auth_server, chromeos::Uri(),
                                         StatusCode::kInvalidURL);
      return StatusCode::kInvalidURL;
    }
    std::unique_ptr<AuthorizationZone> auth_zone =
        auth_zone_creator_.Run(auth_server, /*client_id=*/"");
    if (sync_bridge_->IsInitialized()) {
      if (!base::Contains(servers_, auth_server)) {
        servers_.emplace(auth_server, std::move(auth_zone));
        sync_bridge_->AddAuthorizationServer(auth_server);
      }
    } else {
      if (!base::Contains(waiting_servers_, auth_server)) {
        waiting_servers_[auth_server].server = std::move(auth_zone);
      }
    }
    PRINTER_LOG(USER) << BuildLogEntry(__func__, auth_server, chromeos::Uri(),
                                       StatusCode::kOK);
    return StatusCode::kOK;
  }

  void InitAuthorization(const GURL& auth_server,
                         const std::string& scope,
                         StatusCallback callback) override {
    PRINTER_LOG(USER) << BuildLogEntry(__func__, auth_server, chromeos::Uri(),
                                       absl::nullopt, "scope=" + scope);
    AddLoggingToCallback(callback, __func__, auth_server);
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);

    if (!zone) {
      auto it = waiting_servers_.find(auth_server);
      if (it == waiting_servers_.end()) {
        std::move(callback).Run(StatusCode::kUntrustedAuthorizationServer, "");
      } else {
        it->second.init_calls.emplace_back(
            InitAuthorizationCall{scope, std::move(callback)});
      }
      return;
    }

    zone->InitAuthorization(scope, std::move(callback));
  }

  void FinishAuthorization(const GURL& auth_server,
                           const GURL& redirect_url,
                           StatusCallback callback) override {
    PRINTER_LOG(USER) << BuildLogEntry(__func__, auth_server);
    AddLoggingToCallback(callback, __func__, auth_server);

    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      const StatusCode code = base::Contains(waiting_servers_, auth_server)
                                  ? StatusCode::kAuthorizationNeeded
                                  : StatusCode::kUntrustedAuthorizationServer;
      std::move(callback).Run(code, "");
      return;
    }

    zone->FinishAuthorization(redirect_url, std::move(callback));
  }

  void GetEndpointAccessToken(const GURL& auth_server,
                              const chromeos::Uri& ipp_endpoint,
                              const std::string& scope,
                              StatusCallback callback) override {
    PRINTER_LOG(USER) << BuildLogEntry(__func__, auth_server, ipp_endpoint,
                                       absl::nullopt, "scope=" + scope);
    AddLoggingToCallback(callback, __func__, auth_server, ipp_endpoint);

    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      const StatusCode code = base::Contains(waiting_servers_, auth_server)
                                  ? StatusCode::kAuthorizationNeeded
                                  : StatusCode::kUntrustedAuthorizationServer;
      std::move(callback).Run(code, "");
      return;
    }

    zone->GetEndpointAccessToken(ipp_endpoint, scope, std::move(callback));
  }

  void MarkEndpointAccessTokenAsExpired(
      const GURL& auth_server,
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) override {
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    PRINTER_LOG(EVENT) << BuildLogEntry(
        __func__, auth_server, ipp_endpoint,
        zone ? StatusCode::kOK : StatusCode::kUntrustedAuthorizationServer);
    if (zone) {
      zone->MarkEndpointAccessTokenAsExpired(ipp_endpoint,
                                             endpoint_access_token);
    }
  }

 private:
  struct InitAuthorizationCall {
    std::string scope;
    StatusCallback callback;
  };
  struct WaitingServer {
    std::unique_ptr<AuthorizationZone> server;
    std::vector<InitAuthorizationCall> init_calls;
  };

  // Returns a pointer to the corresponding element in `servers_` or nullptr if
  // `auth_server` is untrusted.
  AuthorizationZone* GetAuthorizationZone(const GURL& auth_server) {
    auto it_server = servers_.find(auth_server);
    if (it_server == servers_.end()) {
      return nullptr;
    }
    return it_server->second.get();
  }

  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override {
    return sync_bridge_.get();
  }

  void OnProfileAuthorizationServersInitialized() override {
    for (auto& [url, ws] : waiting_servers_) {
      auto [it, created] = servers_.emplace(url, std::move(ws.server));
      if (created) {
        sync_bridge_->AddAuthorizationServer(url);
      }
      for (InitAuthorizationCall& iac : ws.init_calls) {
        it->second->InitAuthorization(iac.scope, std::move(iac.callback));
      }
    }
    waiting_servers_.clear();
  }

  void OnProfileAuthorizationServersUpdate(std::set<GURL> added,
                                           std::set<GURL> deleted) override {
    for (const GURL& url : deleted) {
      auto it = servers_.find(url);
      if (it == servers_.end()) {
        continue;
      }
      // First, we have to remove the AuthorizationZone from `servers_` to make
      // sure it is not accessed in any callbacks returned by
      // MarkAuthorizationZoneAsUntrusted().
      std::unique_ptr<AuthorizationZone> auth_zone = std::move(it->second);
      servers_.erase(it);
      auth_zone->MarkAuthorizationZoneAsUntrusted();
    }
    for (const GURL& url : added) {
      if (!base::Contains(servers_, url)) {
        servers_.emplace(url, auth_zone_creator_.Run(url, /*client_id=*/""));
      }
    }
  }

  std::map<GURL, WaitingServer> waiting_servers_;
  std::unique_ptr<ProfileAuthServersSyncBridge> sync_bridge_;
  std::map<GURL, std::unique_ptr<AuthorizationZone>> servers_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  CreateAuthZoneCallback auth_zone_creator_;
};

}  // namespace

std::unique_ptr<AuthorizationZonesManager> AuthorizationZonesManager::Create(
    Profile* profile) {
  DCHECK(profile);
  return std::make_unique<AuthorizationZonesManagerImpl>(profile);
}

std::unique_ptr<AuthorizationZonesManager>
AuthorizationZonesManager::CreateForTesting(
    Profile* profile,
    CreateAuthZoneCallback auth_zone_creator,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory store_factory) {
  DCHECK(profile);
  return std::make_unique<AuthorizationZonesManagerImpl>(
      profile, std::move(auth_zone_creator), std::move(change_processor),
      std::move(store_factory));
}

AuthorizationZonesManager::~AuthorizationZonesManager() = default;

AuthorizationZonesManager::AuthorizationZonesManager() = default;

}  // namespace ash::printing::oauth2
