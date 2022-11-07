// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/public_sets.h"
#include "net/first_party_sets/same_party_context.h"
#include "services/network/public/mojom/first_party_sets.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::SiteIndexDataView,
                 net::FirstPartySetEntry::SiteIndex> {
  static uint32_t value(const net::FirstPartySetEntry::SiteIndex& i) {
    return i.value();
  }

  static bool Read(network::mojom::SiteIndexDataView index,
                   net::FirstPartySetEntry::SiteIndex* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    EnumTraits<network::mojom::SiteType, net::SiteType> {
  static network::mojom::SiteType ToMojom(net::SiteType site_type);

  static bool FromMojom(network::mojom::SiteType site_type, net::SiteType* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetEntryDataView,
                 net::FirstPartySetEntry> {
  static net::SchemefulSite primary(const net::FirstPartySetEntry& e) {
    return e.primary();
  }

  static net::SiteType site_type(const net::FirstPartySetEntry& e) {
    return e.site_type();
  }

  static const absl::optional<net::FirstPartySetEntry::SiteIndex>& site_index(
      const net::FirstPartySetEntry& e) {
    return e.site_index();
  }

  static bool Read(network::mojom::FirstPartySetEntryDataView entry,
                   net::FirstPartySetEntry* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    EnumTraits<network::mojom::SamePartyCookieContextType,
               net::SamePartyContext::Type> {
  static network::mojom::SamePartyCookieContextType ToMojom(
      net::SamePartyContext::Type context_type);

  static bool FromMojom(network::mojom::SamePartyCookieContextType context_type,
                        net::SamePartyContext::Type* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::SamePartyContextDataView,
                 net::SamePartyContext> {
  static net::SamePartyContext::Type context_type(
      const net::SamePartyContext& s) {
    return s.context_type();
  }

  static bool Read(network::mojom::SamePartyContextDataView bundle,
                   net::SamePartyContext* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetMetadataDataView,
                 net::FirstPartySetMetadata> {
  static net::SamePartyContext context(const net::FirstPartySetMetadata& m) {
    return m.context();
  }

  static absl::optional<net::FirstPartySetEntry> frame_entry(
      const net::FirstPartySetMetadata& m) {
    return m.frame_entry();
  }

  static absl::optional<net::FirstPartySetEntry> top_frame_entry(
      const net::FirstPartySetMetadata& m) {
    return m.top_frame_entry();
  }

  static bool Read(network::mojom::FirstPartySetMetadataDataView metadata,
                   net::FirstPartySetMetadata* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::PublicFirstPartySetsDataView,
                 net::PublicSets> {
  static const base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>&
  sets(const net::PublicSets& p) {
    return p.entries();
  }

  static const base::flat_map<net::SchemefulSite, net::SchemefulSite>& aliases(
      const net::PublicSets& p) {
    return p.aliases();
  }

  static bool Read(network::mojom::PublicFirstPartySetsDataView public_sets,
                   net::PublicSets* out_public_sets);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetsContextConfigDataView,
                 net::FirstPartySetsContextConfig> {
  static const base::flat_map<net::SchemefulSite,
                              absl::optional<net::FirstPartySetEntry>>&
  customizations(const net::FirstPartySetsContextConfig& config) {
    return config.customizations();
  }

  static bool Read(network::mojom::FirstPartySetsContextConfigDataView config,
                   net::FirstPartySetsContextConfig* out_config);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
