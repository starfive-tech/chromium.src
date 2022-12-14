// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/orchestrator/edit_view_animatee.h"
#import "ios/chrome/browser/ui/orchestrator/location_bar_offset_provider.h"

@protocol BrowserCommands;
@protocol LoadQueryCommands;
@protocol OmniboxCommands;
@protocol OmniboxReturnDelegate;
@class OmniboxViewController;
class OmniboxTextChangeDelegate;

@protocol OmniboxViewControllerDelegate

// Called after the text input mode changes in the OmniboxViewController. This
// means that the active keyboard has changed.
- (void)omniboxViewControllerTextInputModeDidChange:
    (OmniboxViewController*)omniboxViewController;

// Called after the user uses the "Visit copied link" context menu entry.
- (void)omniboxViewControllerUserDidVisitCopiedLink:
    (OmniboxViewController*)omniboxViewController;

// Starts a reverse image search with `image`. Caller must check that the search
// engine supports image search.
- (void)omniboxViewControllerSearchImage:(UIImage*)image;

@end

@interface OmniboxViewController : UIViewController<EditViewAnimatee,
                                                    LocationBarOffsetProvider,
                                                    OmniboxConsumer>

// The textfield used by this view controller.
@property(nonatomic, readonly, strong) OmniboxTextFieldIOS* textField;

// The default leading image to be used on omnibox focus before this is updated
// via OmniboxConsumer protocol.
@property(nonatomic, strong) UIImage* defaultLeadingImage;

// The default leading image to be used whenever the omnibox text is empty.
@property(nonatomic, strong) UIImage* emptyTextLeadingImage;

// The current semantic content attribute for the views this view controller
// manages
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;

// The dispatcher for the paste and go action.
@property(nonatomic, weak)
    id<BrowserCommands, LoadQueryCommands, OmniboxCommands>
        dispatcher;

// The delegate for this object.
@property(nonatomic, weak) id<OmniboxViewControllerDelegate> delegate;
@property(nonatomic, weak) id<OmniboxReturnDelegate> returnKeyDelegate;

// Designated initializer.
- (instancetype)initWithIncognito:(BOOL)isIncognito;

- (void)setTextChangeDelegate:(OmniboxTextChangeDelegate*)textChangeDelegate;

// Hides extra chrome, i.e. attributed text, and clears.
- (void)prepareOmniboxForScribble;
// Restores the chrome post-scribble.
- (void)cleanupOmniboxAfterScribble;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_
