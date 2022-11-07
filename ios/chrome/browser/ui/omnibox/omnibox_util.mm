// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of symbol images.
const CGFloat kSymbolLocationBarPointSize = 10;

}  // namespace

#pragma mark - Suggestion icons.

OmniboxSuggestionIconType GetOmniboxSuggestionIconTypeForAutocompleteMatchType(
    AutocompleteMatchType::Type type,
    bool is_starred) {
  if (is_starred)
    return OmniboxSuggestionIconType::kBookmark;

  // TODO(crbug.com/1122669): Handle trending zero-prefix suggestions by
  // checking the match subtype similar to AutocompleteMatch::GetVectorIcon().

  switch (type) {
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL_DEPRECATED:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::OPEN_TAB:
    case AutocompleteMatchType::HISTORY_CLUSTER:
    case AutocompleteMatchType::STARTER_PACK:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
      return OmniboxSuggestionIconType::kDefaultFavicon;
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::VOICE_SUGGEST:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
      return OmniboxSuggestionIconType::kSearch;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return OmniboxSuggestionIconType::kSearchHistory;
    case AutocompleteMatchType::CALCULATOR:
      return OmniboxSuggestionIconType::kCalculator;
    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::NULL_RESULT_MESSAGE:
    case AutocompleteMatchType::NUM_TYPES:
      NOTREACHED();
      return OmniboxSuggestionIconType::kDefaultFavicon;
  }
}

UIImage* GetOmniboxSuggestionIconForAutocompleteMatchType(
    AutocompleteMatchType::Type type,
    bool is_starred) {
  OmniboxSuggestionIconType iconType =
      GetOmniboxSuggestionIconTypeForAutocompleteMatchType(type, is_starred);
  return GetOmniboxSuggestionIcon(iconType);
}

#pragma mark - Security icons.

// Returns the asset with "always template" rendering mode.
UIImage* GetLocationBarSecurityIcon(LocationBarSecurityIconType iconType) {
  if (UseSymbols()) {
    return DefaultSymbolTemplateWithPointSize(
        GetLocationBarSecuritySymbolName(iconType),
        kSymbolLocationBarPointSize);
  }
  NSString* imageName = GetLocationBarSecurityIconTypeAssetName(iconType);
  return [[UIImage imageNamed:imageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

// Converts the `security_level` to an appropriate security icon type.
LocationBarSecurityIconType GetLocationBarSecurityIconTypeForSecurityState(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::NONE:
      return INFO;
    case security_state::WARNING:
    case security_state::DANGEROUS:
      return NOT_SECURE_WARNING;
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return SECURE;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return LOCATION_BAR_SECURITY_ICON_TYPE_COUNT;
  }
}

// Converts the `security_level` to an appropriate icon in "always template"
// rendering mode.
UIImage* GetLocationBarSecurityIconForSecurityState(
    security_state::SecurityLevel security_level) {
  LocationBarSecurityIconType iconType =
      GetLocationBarSecurityIconTypeForSecurityState(security_level);
  return GetLocationBarSecurityIcon(iconType);
}
