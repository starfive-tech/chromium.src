// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

PrivacySandboxSettingsFactory* PrivacySandboxSettingsFactory::GetInstance() {
  return base::Singleton<PrivacySandboxSettingsFactory>::get();
}

privacy_sandbox::PrivacySandboxSettings*
PrivacySandboxSettingsFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxSettings*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxSettingsFactory::PrivacySandboxSettingsFactory()
    : ProfileKeyedServiceFactory(
          "PrivacySandboxSettings",
          ProfileSelections::BuildForRegularAndIncognito()) {}

KeyedService* PrivacySandboxSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return new privacy_sandbox::PrivacySandboxSettings(
      std::make_unique<PrivacySandboxSettingsDelegate>(profile),
      HostContentSettingsMapFactory::GetForProfile(profile),
      CookieSettingsFactory::GetForProfile(profile).get(), profile->GetPrefs(),
      profile->IsIncognitoProfile());
}
