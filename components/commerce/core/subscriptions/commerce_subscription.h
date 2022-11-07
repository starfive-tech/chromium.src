// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

/**
 * To add a new SubscriptionType / IdentifierType / ManagementType:
 * 1. Define the type in the enum class in commerce_subscription.h.
 * 2. Update the conversion methods between the type and std::string in
 * commerce_subscription.cc.
 * 3. Add the corresponding entry in {@link
 * commerce_subscription_db_content.proto} to ensure the storage works
 * correctly.
 */

namespace commerce {

// The type of subscription.
enum class SubscriptionType {
  // Unspecified type.
  kTypeUnspecified = 0,
  // Subscription for price tracking.
  kPriceTrack = 1,
};

// The type of subscription identifier.
enum class IdentifierType {
  // Unspecified identifier type.
  kIdentifierTypeUnspecified = 0,
  // Offer id identifier, used in chrome-managed price tracking.
  kOfferId = 1,
  // Product cluster id identifier, used in user-managed price tracking.
  kProductClusterId = 2,
};

// The type of subscription management.
enum class ManagementType {
  // Unspecified management type.
  kTypeUnspecified = 0,
  // Automatic chrome-managed subscription.
  kChromeManaged = 1,
  // Explicit user-managed subscription.
  kUserManaged = 2,
};

struct UserSeenOffer {
  UserSeenOffer(std::string offer_id,
                long user_seen_price,
                std::string country_code);
  UserSeenOffer(const UserSeenOffer&);
  UserSeenOffer& operator=(const UserSeenOffer&);
  ~UserSeenOffer();

  std::string offer_id;
  long user_seen_price;
  std::string country_code;
};

extern const int64_t kUnknownSubscriptionTimestamp;

struct CommerceSubscription {
  CommerceSubscription(
      SubscriptionType type,
      IdentifierType id_type,
      std::string id,
      ManagementType management_type,
      int64_t timestamp = kUnknownSubscriptionTimestamp,
      absl::optional<UserSeenOffer> user_seen_offer = absl::nullopt);
  CommerceSubscription(const CommerceSubscription&);
  CommerceSubscription& operator=(const CommerceSubscription&);
  ~CommerceSubscription();

  SubscriptionType type;
  IdentifierType id_type;
  std::string id;
  ManagementType management_type;
  int64_t timestamp;
  absl::optional<UserSeenOffer> user_seen_offer;
};

std::string SubscriptionTypeToString(SubscriptionType type);
SubscriptionType StringToSubscriptionType(const std::string& s);
std::string SubscriptionIdTypeToString(IdentifierType type);
IdentifierType StringToSubscriptionIdType(const std::string& s);
std::string SubscriptionManagementTypeToString(ManagementType type);
ManagementType StringToSubscriptionManagementType(const std::string& s);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_
