// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/app_registration_waiter.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

namespace web_app {

AppTypeInitializationWaiter::AppTypeInitializationWaiter(Profile* profile,
                                                         apps::AppType app_type)
    : app_type_(app_type) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  Observe(&cache);

  if (cache.IsAppTypeInitialized(app_type))
    run_loop_.Quit();
}

AppTypeInitializationWaiter::~AppTypeInitializationWaiter() = default;

void AppTypeInitializationWaiter::Await() {
  run_loop_.Run();
}

void AppTypeInitializationWaiter::OnAppUpdate(const apps::AppUpdate& update) {}

void AppTypeInitializationWaiter::OnAppTypeInitialized(apps::AppType app_type) {
  if (app_type == app_type_)
    run_loop_.Quit();
}

void AppTypeInitializationWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

AppRegistrationWaiter::AppRegistrationWaiter(Profile* profile,
                                             const AppId& app_id,
                                             apps::Readiness readiness)
    : app_id_(app_id), readiness_(readiness) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  Observe(&cache);
  cache.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
    if (update.Readiness() == readiness_)
      run_loop_.Quit();
  });
}
AppRegistrationWaiter::~AppRegistrationWaiter() = default;

void AppRegistrationWaiter::Await() {
  run_loop_.Run();
}

void AppRegistrationWaiter::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == app_id_ && update.Readiness() == readiness_)
    run_loop_.Quit();
}
void AppRegistrationWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

}  // namespace web_app
