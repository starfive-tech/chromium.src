// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "content/common/content_export.h"
#include "net/first_party_sets/public_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// FirstPartySetsLoader loads information about First-Party Sets (specified
// here: https://github.com/privacycg/first-party-sets) into a members-to-owners
// map asynchronously and returns it with a callback. It requires input sources
// from the component updater via `SetComponentSets`, and the command line via
// `SetManuallySpecifiedSet`.
class CONTENT_EXPORT FirstPartySetsLoader {
 public:
  using LoadCompleteOnceCallback = base::OnceCallback<void(net::PublicSets)>;
  using FlattenedSets = FirstPartySetParser::SetsMap;
  using SingleSet = FirstPartySetParser::SingleSet;

  explicit FirstPartySetsLoader(LoadCompleteOnceCallback on_load_complete);

  ~FirstPartySetsLoader();

  FirstPartySetsLoader(const FirstPartySetsLoader&) = delete;
  FirstPartySetsLoader& operator=(const FirstPartySetsLoader&) = delete;

  // Stores the First-Party Set that was provided via the `kUseFirstPartySet`
  // flag/switch.
  void SetManuallySpecifiedSet(const LocalSetDeclaration& local_set);

  // Asynchronously parses and stores the sets from `sets_file`, and merges with
  // any previously-loaded sets as needed. In case of invalid input, the set of
  // sets provided by the file is considered empty.
  //
  // Only the first call to SetComponentSets can have any effect; subsequent
  // invocations are ignored.
  void SetComponentSets(base::File sets_file);

  // Close the file on thread pool that allows blocking.
  void DisposeFile(base::File sets_file);

 private:
  // Parses the contents of `raw_sets` as a collection of First-Party Set
  // declarations, and stores the result.
  void OnReadSetsFile(const std::string& raw_sets);

  // Modifies `public_sets_` to include the CLI-provided set, if any. Must not
  // be called until the loader has received the CLI flag value via
  // `SetManuallySpecifiedSet`, and the public sets via `SetComponentSets`.
  void ApplyManuallySpecifiedSet();

  // Checks the required inputs have been received, and if so, invokes the
  // callback `on_load_complete_`, after merging sets appropriately.
  void MaybeFinishLoading();

  // Returns true if all sources are present (Component Updater sets, CLI set).
  bool HasAllInputs() const;

  // Holds the public First-Party Sets. This is nullopt until received from
  // Component Updater. It may be modified based on the manually-specified set.
  absl::optional<net::PublicSets> public_sets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the set that was provided on the command line (if any). This is
  // nullopt until `SetManuallySpecifiedSet` is called.
  absl::optional<LocalSetDeclaration> manually_specified_set_
      GUARDED_BY_CONTEXT(sequence_checker_);

  enum Progress {
    kNotStarted,
    kStarted,
    kFinished,
  };

  Progress component_sets_parse_progress_
      GUARDED_BY_CONTEXT(sequence_checker_) = kNotStarted;

  // We use a OnceCallback to ensure we only pass along the completed sets once.
  LoadCompleteOnceCallback on_load_complete_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for latency metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_
