// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"
#include "base/feature_list.h"

namespace switches {

// All switches in alphabetical order.

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kAccountIdMigration{"AccountIdMigration",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
// If enabled, child accounts (i.e. Unicorn accounts) on Android do not have the
// Sync feature forced on.
const base::Feature kAllowSyncOffForChildAccounts{
    "AllowSyncOffForChildAccounts", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, SigninChecker is created before displaying the sync consent
// fragment during FRE.
//
// This should have no user-visible impact, the flag is present as a
// kill-switch.
const base::Feature kCreateSigninCheckerBeforeSyncConsentFragment{
    "CreateSigninCheckerBeforeSyncConsentFragment",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// If enabled, performs the URL-based check first when proving that the
// X-Chrome-Connected header is not needed in request headers on HTTP
// redirects. The hypothesis is that this order of checks is faster to perform.
const base::Feature kNewSigninRequestHeaderCheckOrder{
    "NewSigninRequestHeaderCheckOrder", base::FEATURE_DISABLED_BY_DEFAULT};

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables sending signin scoped device id to LSO with refresh token request.
const char kDisableSigninScopedDeviceId[] = "disable-signin-scoped-device-id";

// Enables fetching account capabilities and populating AccountInfo with the
// fetch result.
// Disabled on iOS because this platform doesn't have a compatible
// `AccountCapabilitiesFetcher` implementation yet.
// TODO(https://crbug.com/1305191): implement feature on iOS.
#if BUILDFLAG(IS_IOS)
const base::Feature kEnableFetchingAccountCapabilities{
    "EnableFetchingAccountCapabilities", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kEnableFetchingAccountCapabilities{
    "EnableFetchingAccountCapabilities", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_IOS)

// Decouples signing out from clearing browsing data on Android. Users are
// no longer signed-out when they clear browsing data. Instead they may
// choose to sign out separately by pressing another button.
// Disabled by default in IOS because the launch process is behind android.
#if BUILDFLAG(IS_ANDROID)
const base::Feature kEnableCbdSignOut{"EnableCbdSignOut",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
#elif BUILDFLAG(IS_IOS)
const base::Feature kEnableCbdSignOut{"EnableCbdSignOut",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// This feature disables all extended sync promos.
const base::Feature kForceDisableExtendedSyncPromos{
    "ForceDisableExtendedSyncPromos", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Features to trigger the startup sign-in promo at boot.
const base::Feature kForceStartupSigninPromo{"ForceStartupSigninPromo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTangibleSync{"TangibleSync",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace switches
