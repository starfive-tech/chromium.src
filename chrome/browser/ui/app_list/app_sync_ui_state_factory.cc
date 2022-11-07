// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_sync_ui_state_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_sync_ui_state.h"
#include "extensions/browser/extension_registry_factory.h"

// static
AppSyncUIState* AppSyncUIStateFactory::GetForProfile(Profile* profile) {
  if (!AppSyncUIState::ShouldObserveAppSyncForProfile(profile))
    return nullptr;

  return static_cast<AppSyncUIState*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppSyncUIStateFactory* AppSyncUIStateFactory::GetInstance() {
  return base::Singleton<AppSyncUIStateFactory>::get();
}

AppSyncUIStateFactory::AppSyncUIStateFactory()
    : ProfileKeyedServiceFactory("AppSyncUIState") {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

AppSyncUIStateFactory::~AppSyncUIStateFactory() {}

KeyedService* AppSyncUIStateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(AppSyncUIState::ShouldObserveAppSyncForProfile(profile));
  return new AppSyncUIState(profile);
}
