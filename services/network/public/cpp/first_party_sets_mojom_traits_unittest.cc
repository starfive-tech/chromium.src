// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/first_party_sets_mojom_traits.h"

#include "base/containers/flat_map.h"
#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/public_sets.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

TEST(FirstPartySetsTraitsTest, Roundtrips_SiteIndex) {
  net::FirstPartySetEntry::SiteIndex original(1337);
  net::FirstPartySetEntry::SiteIndex round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SiteIndex>(
      original, round_tripped));

  EXPECT_EQ(original, round_tripped);
}

TEST(FirstPartySetsTraitsTest, Roundtrips_SiteType) {
  for (net::SiteType site_type : {
           net::SiteType::kPrimary,
           net::SiteType::kAssociated,
           net::SiteType::kService,
       }) {
    net::SiteType roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SiteType>(
        site_type, roundtrip));
    EXPECT_EQ(site_type, roundtrip);
  }
}

TEST(FirstPartySetsTraitsTest, Roundtrips_FirstPartySetEntry) {
  net::SchemefulSite primary(GURL("https://primary.test"));

  net::FirstPartySetEntry original(primary, net::SiteType::kAssociated, 1);
  net::FirstPartySetEntry round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FirstPartySetEntry>(
      original, round_tripped));

  EXPECT_EQ(original, round_tripped);
  EXPECT_EQ(round_tripped.primary(), primary);
}

TEST(FirstPartySetsTraitsTest, Roundtrips_SamePartyCookieContextType) {
  using ContextType = net::SamePartyContext::Type;
  for (ContextType context_type :
       {ContextType::kCrossParty, ContextType::kSameParty}) {
    ContextType roundtrip;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::SamePartyCookieContextType>(
            context_type, roundtrip));
    EXPECT_EQ(context_type, roundtrip);
  }
}

TEST(FirstPartySetsTraitsTest, RoundTrips_SamePartyContext) {
  {
    net::SamePartyContext same_party(net::SamePartyContext::Type::kSameParty);
    net::SamePartyContext copy;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SamePartyContext>(
        same_party, copy));
    EXPECT_EQ(copy.context_type(), net::SamePartyContext::Type::kSameParty);
  }

  {
    net::SamePartyContext cross_party(net::SamePartyContext::Type::kCrossParty);
    net::SamePartyContext copy;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SamePartyContext>(
        cross_party, copy));
    EXPECT_EQ(copy.context_type(), net::SamePartyContext::Type::kCrossParty);
  }
}

TEST(FirstPartySetsTraitsTest, Roundtrips_FirstPartySetMetadata) {
  net::SchemefulSite frame_owner(GURL("https://frame.test"));
  net::SchemefulSite top_frame_owner(GURL("https://top_frame.test"));

  net::FirstPartySetEntry frame_entry(frame_owner, net::SiteType::kAssociated,
                                      1);
  net::FirstPartySetEntry top_frame_entry(top_frame_owner,
                                          net::SiteType::kAssociated, 2);

  auto make_metadata = [&]() {
    // Use non-default values to ensure serialization/deserialization works
    // properly.
    return net::FirstPartySetMetadata(
        net::SamePartyContext(net::SamePartyContext::Type::kSameParty),
        &frame_entry, &top_frame_entry);
  };

  net::FirstPartySetMetadata original = make_metadata();
  net::FirstPartySetMetadata round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::FirstPartySetMetadata>(original, round_tripped));

  EXPECT_EQ(round_tripped.context(),
            net::SamePartyContext(net::SamePartyContext::Type::kSameParty));
  EXPECT_EQ(round_tripped.frame_entry(), frame_entry);
  EXPECT_EQ(round_tripped.top_frame_entry(), top_frame_entry);

  EXPECT_EQ(round_tripped, make_metadata());
}

TEST(FirstPartySetsTraitsTest, RoundTrips_PublicFirstPartySets) {
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite b_cctld(GURL("https://b.cctld"));
  net::SchemefulSite c(GURL("https://c.test"));

  const net::PublicSets original(
      {
          {a,
           net::FirstPartySetEntry(a, net::SiteType::kPrimary, absl::nullopt)},
          {b, net::FirstPartySetEntry(a, net::SiteType::kAssociated, 0)},
          {c,
           net::FirstPartySetEntry(a, net::SiteType::kService, absl::nullopt)},
      },
      {{b_cctld, b}});

  net::PublicSets round_tripped;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::PublicFirstPartySets>(
          original, round_tripped));

  EXPECT_EQ(original, round_tripped);
  EXPECT_FALSE(round_tripped.entries().empty());
  EXPECT_FALSE(round_tripped.aliases().empty());
}

TEST(FirstPartySetsTraitsTest, RoundTrips_FirstPartySetsContextConfig) {
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite c(GURL("https://c.test"));

  const net::FirstPartySetsContextConfig original({
      {a, net::FirstPartySetEntry(a, net::SiteType::kPrimary, absl::nullopt)},
      {b, net::FirstPartySetEntry(a, net::SiteType::kAssociated, 0)},
      {c, absl::nullopt},
  });

  net::FirstPartySetsContextConfig round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::FirstPartySetsContextConfig>(original,
                                                           round_tripped));

  EXPECT_EQ(original, round_tripped);
}

}  // namespace
}  // namespace network
