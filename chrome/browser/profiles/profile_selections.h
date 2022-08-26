// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

// The class `ProfileSelections` and enum `ProfileSelection` are not coupled
// with the usage of `ProfileKeyedServiceFactory`, however the experiment of
// changing the default value of `ProfileSelections` behavior is mainly done for
// the `ProfileKeyedServiceFactory`.
// If other usages are needed it is best not to use the builders that contains
// experimental code (mentioned below).

// Enum used to map the logic of selecting the right profile based on the given
// profile.
enum class ProfileSelection {
  kNone,                  // Original: No Profile  --  OTR: No Profile
  kOriginalOnly,          // Original: Self        --  OTR: No Profile
  kOwnInstance,           // Original: Self        --  OTR: Self
  kRedirectedToOriginal,  // Original: Self        --  OTR: Original
  kOffTheRecordOnly       // Original: No Profile  --  OTR: Self
};

// Contains the logic for ProfileSelection for the different main Profile types
// (Regular, Guest and System). Each of these profile types also have Off the
// Record profiles equivalent, e.g. Incognito is Off the Record profile for
// Regular profile, the Guest user-visible profile is off-the-record, the
// Profile Picker uses the off-the-record System Profile.
// Maps Profile types to `ProfileSelection`.
class ProfileSelections {
 public:
  ProfileSelections(const ProfileSelections& other);
  ~ProfileSelections();

  // - Predefined `ProfileSelections`

  // Regular builders (independent of the experiments):

  // All Profiles are selected.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | self       |
  // | Guest   | self       | self       |
  // | System  | self       | self       |
  // +---------+------------+------------+
  static ProfileSelections BuildForAllProfiles();

  // No Profiles are selected.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | no profile | no profile |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildNoProfilesSelected();

  // Only select the regular profile.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | no profile |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildForRegularProfile();

  // Redirects incognito profiles to their original regular profile. No
  // profiles for Guest and System profiles. "NonExperimental" is added to
  // differentiate with the experimental behavior during the experiment, once
  // done it will be the equivalent builder.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildRedirectedInIncognitoNonExperimental();

  // Redirects all OTR profiles to their original profiles.
  // Includes all profile types (Regular, Guest and System).
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | self       | original   |
  // | System  | self       | original   |
  // +---------+------------+------------+
  static ProfileSelections BuildRedirectedToOriginal();

  // Experimental builders:
  //
  // Experimental builders (should only be used for the transition from
  // `BrowserContextKeyedServiceFactory` to `ProfileKeyedServiceFactory`):
  // The following builder will contain experimental code indirectly, by not
  // giving a value to Guest and System Profile (unless forced by parameters).
  // The experiment is targeted to affect usages on
  // `ProfileKeyedServiceFactory`. During the experiment phase, these builders
  // will not have very accurate function names, the name is based on the end
  // result behavior the experiment enforced behavior. With/Without experiment
  // behavior is described per experimental builder. The parameters force_* will
  // allow to have an easier transition when adapting to the experiment.

  // Default implementation, without the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | no profile |
  // | Guest   | self       | no profile |
  // | System  | self       | no profile |
  // +---------+------------+------------+
  //
  // After the migration(crbug.com/1284664) this default behaviour will change.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | no profile |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // +---------+------------+------------+
  //
  // Parameters: (used during the experiment)
  // - force_guest: true, force Guest with `ProfileSelection::kOriginalOnly`.
  // - force_system: true, force System with `ProfileSelection::kOriginalOnly`.
  static ProfileSelections BuildDefault(bool force_guest = false,
                                        bool force_system = false);

  // Without the experiment:
  // - Returns Regular for Regular, incognito and other regular OTR profiles.
  // - Returns Guest Original for GuestOriginal  and GuestOTR (same as Regular).
  // - Returns System Original for SystemOriginal  and SystemOTR (same as
  // Regular).
  //
  // With the experiment:
  // - Returns Regular for Regular, incognito and other regular OTR profiles.
  // - Return nullptr for all Guest and System profiles.
  //
  // Without the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | self       | original   |
  // | System  | self       | original   |
  // +---------+------------+------------+
  //
  // With the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // +---------+------------+------------+
  //
  // Parameters: (used during the experiment)
  // - force_guest: true, force Guest with
  // `ProfileSelecion::kRedirectedToOriginal`.
  // - force_system: true, force System with
  // `ProfileSelecion::kRedirectedToOriginal`.
  static ProfileSelections BuildRedirectedInIncognito(
      bool force_guest = false,
      bool force_system = false);

  // Without the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | self       |
  // | Guest   | self       | self       |
  // | System  | self       | self       |
  // +---------+------------+------------+
  //
  // With the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | self       |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // +---------+------------+------------+
  //
  // Parameters: (used during the experiment)
  // - force_guest: true, force Guest with `ProfileSelecion::kOwnInstance`.
  // - force_system: true, force System with `ProfileSelecion::kOwnInstance`.
  static ProfileSelections BuildForRegularAndIncognito(
      bool force_guest = false,
      bool force_system = false);

  // Builder to construct the `ProfileSelections` parameters.
  class Builder {
   public:
    Builder();
    ~Builder();

    // Builder setters
    Builder& WithRegular(ProfileSelection selection);
    // Note: When Guest and Regular are not mutually exclusive on Ash and
    // Lacros, a Profile can potentially return true for both
    // `IsRegularProfile()` and `IsGuestSession()`. This is currently not
    // supported by the API, meaning that extra code might need to be added to
    // make sure all the cases are properly covered. Using the API, if both
    // `IsRegularProfile()` and `IsGuestSession()` are true, Regular
    // ProfileSelection logic will be used.
    // TODO(crbug.com/1348572): remove this comment once `IsGuestSession()` is
    // fixed.
    Builder& WithGuest(ProfileSelection selection);
    Builder& WithSystem(ProfileSelection selection);

    // Builds the `ProfileSelections`.
    ProfileSelections Build();

   private:
    std::unique_ptr<ProfileSelections> selections_;
  };

  // Given a Profile and a ProfileSelection enum, returns the right profile
  // (can potentially return nullptr).
  Profile* ApplyProfileSelection(Profile* profile) const;

 private:
  // Default constructor settings sets Regular Profile ->
  // `ProfileSelection::kOriginalOnly`. It should be constructed through the
  // Builder. Value for Guest and System profile not being overridden will
  // default to the behaviour of Regular Profile.
  ProfileSelections();

  void SetProfileSelectionForRegular(ProfileSelection selection);
  void SetProfileSelectionForGuest(ProfileSelection selection);
  void SetProfileSelectionForSystem(ProfileSelection selection);

  // Returns the `ProfileSelection` based on the profile information through the
  // set mapping.
  ProfileSelection GetProfileSelection(Profile* profile) const;

  // Default value for the mapping of
  // Regular Profile -> `ProfileSelection::kOriginalOnly`
  // Not assigning values for Guest and System Profiles now defaults to the
  // behavior of regular profiles. This will change later on to default to
  // `ProfileSelection::kNone`.
  ProfileSelection regular_profile_selection_ = ProfileSelection::kOriginalOnly;
  absl::optional<ProfileSelection> guest_profile_selection_;
  absl::optional<ProfileSelection> system_profile_selection_;
};

#endif  // !CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
