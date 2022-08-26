// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_FACTORY_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class FastCheckoutCapabilitiesFetcher;

namespace content {
class BrowserContext;
}  // namespace content

// Factory for `FastCheckoutCapabilitiesFetcher` instances for a given
// `BrowserContext`.
class FastCheckoutCapabilitiesFetcherFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static FastCheckoutCapabilitiesFetcherFactory* GetInstance();

  FastCheckoutCapabilitiesFetcherFactory();
  ~FastCheckoutCapabilitiesFetcherFactory() override;

  static FastCheckoutCapabilitiesFetcher* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_FACTORY_H_
