// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/common/content_features.h"

namespace web_app {

// static
IsolatedWebAppReaderRegistry*
IsolatedWebAppReaderRegistryFactory::GetForProfile(Profile* profile) {
  return static_cast<IsolatedWebAppReaderRegistry*>(
      IsolatedWebAppReaderRegistryFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IsolatedWebAppReaderRegistryFactory*
IsolatedWebAppReaderRegistryFactory::GetInstance() {
  return base::Singleton<IsolatedWebAppReaderRegistryFactory>::get();
}

IsolatedWebAppReaderRegistryFactory::IsolatedWebAppReaderRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "IsolatedWebAppReaderRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

IsolatedWebAppReaderRegistryFactory::~IsolatedWebAppReaderRegistryFactory() =
    default;

KeyedService* IsolatedWebAppReaderRegistryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new IsolatedWebAppReaderRegistry(
      std::make_unique<IsolatedWebAppValidator>());
}

content::BrowserContext*
IsolatedWebAppReaderRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return nullptr;
  }

  return GetBrowserContextForWebApps(context);
}

}  // namespace web_app
