// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_

#import <Foundation/Foundation.h>
#import <map>
#import <set>
#import <vector>

#import "base/containers/small_map.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

class PromosManagerTest;

// Centralized promos manager for coordinating and scheduling the display of
// app-wide promos. Feature teams interested in displaying promos should
// leverage this manager.
class PromosManager {
 public:
  explicit PromosManager(PrefService* local_state);
  ~PromosManager();

  // Records the impression of `promo` in the impression history.
  void RecordImpression(promos_manager::Promo promo);

  // Ingests promo-specific impression limits and stores them in-memory for
  // later reference.
  void InitializePromoImpressionLimits(
      base::small_map<
          std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>
          promo_impression_limits);

  // Returns the next promo for display, if any.
  absl::optional<promos_manager::Promo> NextPromoForDisplay() const;

  // Registers `promo` for continuous display, and persists registration status
  // across app launches.
  void RegisterPromoForContinuousDisplay(promos_manager::Promo promo);

  // Registers `promo` for single (one-time) display, and persists registration
  // status across app launches.
  void RegisterPromoForSingleDisplay(promos_manager::Promo promo);

  // Deregisters `promo` (stopping `promo` from being displayed) by removing the
  // promo entry from the single-display and continuous-display active promos
  // lists.
  void DeregisterPromo(promos_manager::Promo promo);

  // Initialize the Promos Manager by restoring state from Prefs. Must be called
  // after creation and before any other operation.
  void Init();

 private:
  // Weak pointer to the local state prefs store.
  const raw_ptr<PrefService> local_state_;

  // The set of currently active, continuous-display promos.
  std::set<promos_manager::Promo> active_promos_;

  // The set of currently active, single-display promos.
  std::set<promos_manager::Promo> single_display_active_promos_;

  // The impression history sorted by `day` (most recent -> least recent).
  std::vector<promos_manager::Impression> impression_history_;

  // Promo-specific impression limits (promos_manager::Promo : [Impression
  // Limits]).
  base::small_map<std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>
      promo_impression_limits_;

  // `promo`-specific impression limits, if defined. May return an empty
  // NSArray, indicating no promo-specific impression limits were defined for
  // `promo`.
  NSArray<ImpressionLimit*>* PromoImpressionLimits(
      promos_manager::Promo promo) const;

  // Returns a std::vector<promos_manager::Promo> that only includes promos in
  // `active_promos`, sorted by day (least recent -> most recent).
  //
  // Assumes that `sorted_impressions` is sorted by day (most recent -> least
  // recent).
  //
  // When `active_promos` or `sorted_impressions` is empty, returns an empty
  // array.
  //
  // Promos that have never been shown before are considered less recently shown
  // than promos that have been shown.
  //
  // Promos shown on the same day will be sorted by relative position. More
  // concretely, even if two promos are shown on the same day, the promos with
  // the lower index in the impressions history list will be considered more
  // recently seen, as `sorted_impressions` is sorted by day (most recent ->
  // least recent).
  std::vector<promos_manager::Promo> LeastRecentlyShown(
      const std::set<promos_manager::Promo>& active_promos,
      const std::vector<promos_manager::Impression>& sorted_impressions) const;

  // Impression limits that count against all promos.
  NSArray<ImpressionLimit*>* GlobalImpressionLimits() const;

  // Impression limits that count against any given promo.
  NSArray<ImpressionLimit*>* GlobalPerPromoImpressionLimits() const;

  // Loops over the stored impression history (base::Value::List) and returns
  // corresponding a std::vector<promos_manager::Impression>.
  std::vector<promos_manager::Impression> ImpressionHistory(
      const base::Value::List& stored_impression_history);

  // Loops over the stored active promos list (base::Value::List) and returns
  // a corresponding std::set<promos_manager::Promo>.
  std::set<promos_manager::Promo> ActivePromos(
      const base::Value::List& stored_active_promos);

  // Returns true if any impression limit from `impression_limits` is triggered,
  // and false otherwise.
  //
  // At each limit, evaluates the following:
  //
  // (1) Is the current limit valid for evaluation? This is determined by
  // whether or not `window_days` is < the current limit's window.
  //
  // (2) If the limit is valid for evaluation, compare `impression_count` with
  // the current limit's impression count. If `impression_count` >= the current
  // limit's impression count, the limit has been triggered.

  // (3) If the limit is triggered, exits early and returns true. Otherwise,
  // keep going.
  bool AnyImpressionLimitTriggered(
      int impression_count,
      int window_days,
      NSArray<ImpressionLimit*>* impression_limits) const;

  // Algorithm loops over pre-sorted impressions history list. The algorithm
  // assumes `valid_impressions` is sorted by impression day (most recent ->
  // least recent).
  //
  // At each impression, the algorithm asks if either a time-based or
  // time-agnostic impression limit has been met. If so, the algorithm exits
  // early and returns false.
  //
  // If the algorithm reaches its end, no impression limits were hit for
  // `promo`. If so, the algorithm returns true, as it's safe to display
  // `promo`.
  bool CanShowPromo(
      promos_manager::Promo promo,
      const std::vector<promos_manager::Impression>& sorted_impressions) const;

  // Returns a list of impression counts (std::vector<int>) from a promo
  // impression counts map.
  std::vector<int> ImpressionCounts(
      std::map<promos_manager::Promo, int>& promo_impression_counts) const;

  // Returns the greatest impression count (int) from a promo impression counts
  // map.
  int MaxImpressionCount(
      std::map<promos_manager::Promo, int>& promo_impression_counts) const;

  // Returns the total number of impressions (int) from a promo impression
  // counts map.
  int TotalImpressionCount(
      std::map<promos_manager::Promo, int>& promo_impression_counts) const;

  // Allow unit tests to access private methods.
  friend class PromosManagerTest;
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsImpressionCounts);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsEmptyImpressionCounts);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsTotalImpressionCount);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsZeroForTotalImpressionCount);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsMaxImpressionCount);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsZeroForMaxImpressionCount);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           DetectsSingleImpressionLimitTriggered);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           DetectsOneOfMultipleImpressionLimitsTriggered);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           DetectsNoImpressionLimitTriggered);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, DecidesCanShowPromo);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, DecidesCannotShowPromo);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsLeastRecentlyShown);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsLeastRecentlyShownWithSomeInactivePromos);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsLeastRecentlyShownBreakingTies);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsLeastRecentlyShownWithOnlyOnePromoActive);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsLeastRecentlyShownWithoutImpressionHistory);
  FRIEND_TEST_ALL_PREFIXES(
      PromosManagerTest,
      ReturnsEmptyListWhenLeastRecentlyShownHasNoActivePromoCampaigns);
  FRIEND_TEST_ALL_PREFIXES(
      PromosManagerTest,
      ReturnsEmptyListWhenLeastRecentlyShownHasNoImpressionHistory);
  FRIEND_TEST_ALL_PREFIXES(
      PromosManagerTest,
      SortsUnshownPromosBeforeShownPromosForLeastRecentlyShown);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsImpressionHistory);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsBlankImpressionHistoryForBlankPrefs);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsImpressionHistoryBySkippingMalformedEntries);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsActivePromos);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsBlankActivePromosForBlankPrefs);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsActivePromosAndSkipsMalformedData);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           RegistersPromoForContinuousDisplay);
  FRIEND_TEST_ALL_PREFIXES(
      PromosManagerTest,
      RegistersPromoForContinuousDisplayForEmptyActivePromos);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           RegistersAlreadyRegisteredPromoForContinuousDisplay);
  FRIEND_TEST_ALL_PREFIXES(
      PromosManagerTest,
      RegistersAlreadyRegisteredPromoForContinuousDisplayForEmptyActivePromos);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, RegistersPromoForSingleDisplay);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           RegistersPromoForSingleDisplayForEmptyActivePromos);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           RegistersAlreadyRegisteredPromoForSingleDisplay);
  FRIEND_TEST_ALL_PREFIXES(
      PromosManagerTest,
      RegistersAlreadyRegisteredPromoForSingleDisplayForEmptyActivePromos);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           RegistersPromoSpecificImpressionLimits);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, RecordsImpression);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, DeregistersActivePromo);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, DeregistersNonExistentPromo);
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
