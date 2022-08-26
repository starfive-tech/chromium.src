// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace first_party_sets {

class FirstPartySetsPolicyService;
// Singleton that owns FirstPartySetsPolicyService objects and associates them
// with corresponding BrowserContexts.
//
// Listens for the BrowserContext's destruction notification and cleans up the
// associated FirstPartySetsPolicyService.
class FirstPartySetsPolicyServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  FirstPartySetsPolicyServiceFactory(
      const FirstPartySetsPolicyServiceFactory&) = delete;
  FirstPartySetsPolicyServiceFactory& operator=(
      const FirstPartySetsPolicyServiceFactory&) = delete;

  static FirstPartySetsPolicyService* GetForBrowserContext(
      content::BrowserContext* context);

  static FirstPartySetsPolicyServiceFactory* GetInstance();

  // Checks the criteria for applying the First-Party Sets Overrides policy
  // and returns a pointer to a representation of the policy if all criteria are
  // met. If not, this method returns a nullptr.
  //
  // The returned pointer has the same lifetime as anything returned by the
  // PrefService.
  static const base::Value::Dict* GetPolicyIfEnabled(const Profile& profile);

  // Stores `test_factory` to inject test logic into BuildServiceInstanceFor.
  void SetTestingFactoryForTesting(TestingFactory test_factory);

 private:
  friend struct base::DefaultSingletonTraits<
      FirstPartySetsPolicyServiceFactory>;

  FirstPartySetsPolicyServiceFactory();
  ~FirstPartySetsPolicyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_FACTORY_H_
