// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FeedTopSectionMediator () <IdentityManagerObserverBridgeDelegate> {
  // Observes changes in identity.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
}

@property(nonatomic, assign) ChromeBrowserState* browserState;

// Consumer for this mediator.
@property(nonatomic, weak) id<FeedTopSectionConsumer> consumer;

// Whether the signin promo should be shown. When the promo state changes, it
// will call `promoStateChanges:` on the delegate.
@property(nonatomic, assign) BOOL shouldShowSigninPromo;

@end

@implementation FeedTopSectionMediator

// FeedTopSectionViewControllerDelegate
@synthesize signinPromoConfigurator = _signinPromoConfigurator;

- (instancetype)initWithConsumer:(id<FeedTopSectionConsumer>)consumer
                    browserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
    _consumer = consumer;
  }
  return self;
}

- (void)setUp {
  [self updateShouldShowSigninPromo];
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  [_signinPromoMediator disconnect];
  _signinPromoMediator = nil;
  _identityObserverBridge.reset();
}

#pragma mark - Setters

- (void)setShouldShowSigninPromo:(BOOL)shouldShowSigninPromo {
  if (_shouldShowSigninPromo == shouldShowSigninPromo) {
    return;
  }
  _shouldShowSigninPromo = shouldShowSigninPromo;
  if (shouldShowSigninPromo) {
    [self.signinPromoMediator signinPromoViewIsVisible];
  } else {
    // When the sign-in view is closed, the promo state changes, but
    // -[SigninPromoViewMediator signinPromoViewIsHidden] should not be
    // called because it's already hidden.
    if (!self.signinPromoMediator.invalidClosedOrNeverVisible) {
      [self.signinPromoMediator signinPromoViewIsHidden];
    }
  }
  // TODO(crbug.com/1331010): Update pref if needed.

  // Update the view.
  self.consumer.shouldShowSigninPromo = _shouldShowSigninPromo;
}

#pragma mark - FeedTopSectionViewControllerDelegate

- (SigninPromoViewConfigurator*)signinPromoConfigurator {
  if (!_signinPromoConfigurator) {
    _signinPromoConfigurator = [_signinPromoMediator createConfigurator];
  }
  return _signinPromoConfigurator;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (!self.signinPromoMediator.signinInProgress) {
        // User has signed in, stop showing the promo.
        self.shouldShowSigninPromo = NO;
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateShouldShowSigninPromo];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  // Identity was changed: So first figure out if the promo should still
  // appear. Then update it to match the new configurator if it will show.
  [self updateShouldShowSigninPromo];
  if (self.shouldShowSigninPromo) {
    [self.consumer
        updateSigninPromoWithConfigurator:[self signinPromoConfigurator]];
  }
}

- (void)signinDidFinish {
  [self updateShouldShowSigninPromo];
  if (self.shouldShowSigninPromo) {
    [self.consumer
        updateSigninPromoWithConfigurator:[self signinPromoConfigurator]];
  }
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  self.shouldShowSigninPromo = NO;
  // TODO(crbug.com/1331010): Update local prefs if needed.
}

#pragma mark - Private

- (void)updateShouldShowSigninPromo {
  DCHECK(self.browserState);
  self.shouldShowSigninPromo = NO;
  // Don't show the promo for incognito or start surface.
  if (self.browserState->IsOffTheRecord() ||
      [self.ntpDelegate isStartSurface]) {
    return;
  }
  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO
                                          prefService:_browserState
                                                          ->GetPrefs()]) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
    self.shouldShowSigninPromo =
        !identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  }
}

@end
