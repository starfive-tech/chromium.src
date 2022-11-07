// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {
class PasswordsPrivateDelegate;

// Factory for creating PasswordPrivateDelegates.
class PasswordsPrivateDelegateFactory : public ProfileKeyedServiceFactory {
 public:
  static PasswordsPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context,
      bool create);

  static PasswordsPrivateDelegateFactory* GetInstance();

 private:
  friend class base::NoDestructor<PasswordsPrivateDelegateFactory>;
  PasswordsPrivateDelegateFactory();
  ~PasswordsPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_FACTORY_H_
