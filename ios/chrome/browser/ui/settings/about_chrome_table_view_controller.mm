// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/about_chrome_table_view_controller.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/settings/cells/version_item.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/terms_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLinks = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLinksCredits = kItemTypeEnumZero,
  ItemTypeLinksTerms,
  ItemTypeLinksPrivacy,
  ItemTypeVersion,
};

const CGFloat kDefaultHeight = 70;

}  // namespace

@interface AboutChromeTableViewController ()<VersionFooterDelegate>
@end

@implementation AboutChromeTableViewController

#pragma mark - Public

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_ABOUT_PRODUCT_NAME);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = kDefaultHeight;
  self.tableView.sectionFooterHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionFooterHeight = kDefaultHeight;
  self.styler.cellTitleColor = [UIColor colorNamed:kTextPrimaryColor];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierLinks];

  TableViewDetailTextItem* credits =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeLinksCredits];
  credits.text = l10n_util::GetNSString(IDS_IOS_OPEN_SOURCE_LICENSES);
  credits.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  credits.accessibilityTraits = UIAccessibilityTraitButton;
  credits.accessibilityIdentifier = kSettingsOpenSourceLicencesCellId;
  [model addItem:credits toSectionWithIdentifier:SectionIdentifierLinks];

  TableViewDetailTextItem* terms =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeLinksTerms];
  terms.text = l10n_util::GetNSString(IDS_IOS_TERMS_OF_SERVICE);
  terms.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  terms.accessibilityTraits = UIAccessibilityTraitButton;
  terms.accessibilityIdentifier = kSettingsTOSCellId;
  [model addItem:terms toSectionWithIdentifier:SectionIdentifierLinks];

  TableViewDetailTextItem* privacy =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeLinksPrivacy];
  privacy.text = l10n_util::GetNSString(IDS_IOS_PRIVACY_POLICY);
  privacy.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  privacy.accessibilityTraits = UIAccessibilityTraitButton;
  privacy.accessibilityIdentifier = kSettingsPrivacyNoticeCellId;
  [model addItem:privacy toSectionWithIdentifier:SectionIdentifierLinks];

  VersionItem* version = [[VersionItem alloc] initWithType:ItemTypeVersion];
  version.text = [self versionDescriptionString];
  version.accessibilityTraits = UIAccessibilityTraitButton;
  [model setFooter:version forSectionWithIdentifier:SectionIdentifierLinks];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAboutSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAboutSettingsBack"));
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footer = [super tableView:tableView viewForFooterInSection:section];
  VersionFooter* versionFooter =
      base::mac::ObjCCastStrict<VersionFooter>(footer);
  versionFooter.delegate = self;
  return footer;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeLinksCredits:
      [self openURL:GURL(kChromeUICreditsURL)];
      break;
    case ItemTypeLinksTerms:
      switch (fre_field_trial::GetNewMobileIdentityConsistencyFRE()) {
        case NewMobileIdentityConsistencyFRE::kTwoSteps:
        case NewMobileIdentityConsistencyFRE::kThreeSteps:
        case NewMobileIdentityConsistencyFRE::kUMADialog:
          [self openURL:GetUnifiedTermsOfServiceURL(false)];
          break;
        case NewMobileIdentityConsistencyFRE::kOld:
          [self openURL:GURL(kChromeUITermsURL)];
          break;
      }
      break;
    case ItemTypeLinksPrivacy:
      [self openURL:GURL(l10n_util::GetStringUTF8(IDS_IOS_PRIVACY_POLICY_URL))];
      break;
    case ItemTypeVersion:
      // Version is a footer, it is not interactable.
      NOTREACHED();
      break;
  }
}

#pragma mark - VersionFooterDelegate

- (void)didTapVersionFooter:(VersionFooter*)footer {
  [[UIPasteboard generalPasteboard] setString:[self versionOnlyString]];
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  NSString* messageText = l10n_util::GetNSString(IDS_IOS_VERSION_COPIED);
  MDCSnackbarMessage* message =
      [MDCSnackbarMessage messageWithText:messageText];
  message.category = @"version copied";
  [self.snackbarCommandsHandler showSnackbarMessage:message bottomOffset:0];
}

#pragma mark - Private methods

- (void)openURL:(GURL)URL {
  BlockToOpenURL(self, self.applicationCommandsHandler)(URL);
}

- (std::string)versionString {
  std::string versionString = version_info::GetVersionNumber();
  std::string versionStringModifier = GetChannelString();
  if (!versionStringModifier.empty()) {
    versionString = versionString + " " + versionStringModifier;
  }
  return versionString;
}

- (NSString*)versionDescriptionString {
  return l10n_util::GetNSStringF(IDS_IOS_VERSION,
                                 base::UTF8ToUTF16([self versionString]));
}

- (NSString*)versionOnlyString {
  return base::SysUTF8ToNSString([self versionString]);
}

@end
