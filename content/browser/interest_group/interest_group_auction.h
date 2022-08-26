// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
struct AuctionConfig;
}

namespace content {

class InterestGroupManagerImpl;

// An InterestGroupAuction Handles running an auction, or a component auction.
// Consumers should use AuctionRunner, which sets up InterestGroupAuction and
// extracts their results. Separate from AuctionRunner so that nested
// InterestGroupAuction can handle component auctions as well as top-level
// auction.
//
// Auctions have three phases, with phase transitions handled by the owner. All
// phases complete asynchronously:
//
// * Loading interest groups phase: This loads interest groups that can
// participate in an auction. Waiting for all component auctions to complete
// this phase before advancing to the next ensures that if any auctions share
// bidder worklets, they'll all be loaded together, and only send out a single
// trusted bidding signals request.
//
// * Bidding/scoring phase: This phase loads bidder and seller worklets,
// generates bids, scores bids, and the highest scoring bid for each component
// auction is passed to its parent auction, which also scores it. When this
// phase completes, the winner will have been decided.
//
// * ReportResult / ReportWin phase: This phase invokes ReportResult() on
// winning seller worklets and ReportWin() in the winning bidder worklet.
class CONTENT_EXPORT InterestGroupAuction
    : public auction_worklet::mojom::ScoreAdClient {
 public:
  // Post auction signals (signals only available after auction completes such
  // as winning bid) for debug loss/win reporting.
  struct PostAuctionSignals {
    PostAuctionSignals() = default;

    // For now, top level post auction signals do not have
    // `highest_scoring_other_bid` or `made_highest_scoring_other_bid`.
    PostAuctionSignals(double winning_bid, bool made_winning_bid)
        : winning_bid(winning_bid), made_winning_bid(made_winning_bid) {}

    PostAuctionSignals(double winning_bid,
                       bool made_winning_bid,
                       double highest_scoring_other_bid,
                       bool made_highest_scoring_other_bid)
        : winning_bid(winning_bid),
          made_winning_bid(made_winning_bid),
          highest_scoring_other_bid(highest_scoring_other_bid),
          made_highest_scoring_other_bid(made_highest_scoring_other_bid) {}

    ~PostAuctionSignals() = default;

    double winning_bid = 0.0;
    bool made_winning_bid = false;
    double highest_scoring_other_bid = 0.0;
    bool made_highest_scoring_other_bid = false;
  };

  // Returns true if `origin` is allowed to use the interest group API. Will be
  // called on worklet / interest group origins before using them in any
  // interest group API.
  using IsInterestGroupApiAllowedCallback = base::RepeatingCallback<bool(
      ContentBrowserClient::InterestGroupApiOperation
          interest_group_api_operation,
      const url::Origin& origin)>;

  // Result of an auction or a component auction. Used for histograms. Only
  // recorded for valid auctions. These are used in histograms, so values of
  // existing entries must not change when adding/removing values, and obsolete
  // values must not be reused.
  enum class AuctionResult {
    // The auction succeeded, with a winning bidder.
    kSuccess = 0,

    // The auction was aborted, due to either navigating away from the frame
    // that started the auction or browser shutdown.
    kAborted = 1,

    // Bad message received over Mojo. This is potentially a security error.
    kBadMojoMessage = 2,

    // The user was in no interest groups that could participate in the auction.
    kNoInterestGroups = 3,

    // The seller worklet failed to load.
    kSellerWorkletLoadFailed = 4,

    // The seller worklet crashed.
    kSellerWorkletCrashed = 5,

    // All bidders failed to bid. This happens when all bidders choose not to
    // bid, fail to load, or crash before making a bid.
    kNoBids = 6,

    // The seller worklet rejected all bids (of which there was at least one).
    kAllBidsRejected = 7,

    // The winning bidder worklet crashed. The bidder must have successfully
    // bid, and the seller must have accepted the bid for this to be logged.
    kWinningBidderWorkletCrashed = 8,

    // The seller is not allowed to use the interest group API.
    kSellerRejected = 9,

    // The component auction completed with a winner, but that winner lost the
    // top-level auction.
    kComponentLostAuction = 10,

    // The component seller worklet with the winning bidder crashed during the
    // reporting phase.
    kWinningComponentSellerWorkletCrashed = 11,

    kMaxValue = kWinningComponentSellerWorkletCrashed
  };

  struct BidState {
    BidState();
    BidState(BidState&&);
    ~BidState();

    // Disable copy and assign, since this struct owns a
    // auction_worklet::mojom::BiddingInterestGroupPtr, and mojo classes are not
    // copiable.
    BidState(BidState&) = delete;
    BidState& operator=(BidState&) = delete;

    // Populates `trace_id` with a new trace ID and logs the first trace event
    // for it.
    void BeginTracing();

    // Logs the final event for `trace_id` and clears it. Automatically called
    // on destruction so trace events are all closed if an auction is cancelled.
    void EndTracing();

    StorageInterestGroup bidder;

    // Holds a reference to the BidderWorklet, once created.
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> worklet_handle;

    // Tracing ID associated with the BidState. A nestable async "Bid" trace
    // event is started for a bid state during the generate and score bid phase
    // when the worklet is requested, and ended once the bid is score, or the
    // bidder worklet fails to bid.
    //
    // Additionally, if the BidState is a winner of a component auction, another
    // "Bid" trace event is created when the top-level auction scores the bid,
    // and ends when scoring is complete.
    //
    // Nested events are logged using this ID both by the Auction and by Mojo
    // bidder and seller worklets, potentially in another process.
    //
    // absl::nullopt means no ID is currently assigned, and there's no pending
    // event.
    absl::optional<uint64_t> trace_id;

    // True if the worklet successfully made a bid.
    bool made_bid = false;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in generateBid().
    // They support post auction signal placeholders in their URL string,
    // for example, "https://example.com/${highestScoringOtherBid}".
    // Placeholders will be replaced by corresponding values. For a component
    // auction, post auction signals are only from the component auction, but
    // not the top-level auction.
    absl::optional<GURL> bidder_debug_loss_report_url;
    absl::optional<GURL> bidder_debug_win_report_url;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in scoreAd(). In the case
    // of a component auction, these are the values from component seller that
    // the scored ad was created in, and post auction signals are from the
    // component auction.
    absl::optional<GURL> seller_debug_loss_report_url;
    absl::optional<GURL> seller_debug_win_report_url;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in scoreAd() from the
    // top-level seller, in the case this bidder was made in a component
    // auction, won it, and was then scored by the top-level seller.
    absl::optional<GURL> top_level_seller_debug_win_report_url;
    absl::optional<GURL> top_level_seller_debug_loss_report_url;
  };

  // Result of generated a bid. Contains information that needs to score a bid
  // and is persisted to the end of the auction if the bidder wins. Largely
  // duplicates auction_worklet::mojom::BidderWorkletBid, with additional
  // information about the bidder.
  struct Bid {
    Bid(std::string ad_metadata,
        double bid,
        GURL render_url,
        std::vector<GURL> ad_components,
        base::TimeDelta bid_duration,
        absl::optional<uint32_t> bidding_signals_data_version,
        const blink::InterestGroup::Ad* bid_ad,
        BidState* bid_state,
        InterestGroupAuction* auction);

    Bid(Bid&);

    ~Bid();

    // These are taken directly from the
    // auction_worklet::mojom::BidderWorkletBid.
    const std::string ad_metadata;
    const double bid;
    const GURL render_url;
    const std::vector<GURL> ad_components;
    const base::TimeDelta bid_duration;
    const absl::optional<uint32_t> bidding_signals_data_version;

    // InterestGroup that made the bid. Owned by the BidState of that
    // InterestGroup.
    const raw_ptr<const blink::InterestGroup> interest_group;

    // Points to the InterestGroupAd within `interest_group`.
    const raw_ptr<const blink::InterestGroup::Ad> bid_ad;

    // `bid_state` of the InterestGroup that made the bid. This should not be
    // written to, except for adding seller debug reporting URLs.
    const raw_ptr<BidState> bid_state;

    // The Auction with the interest group that made this bid. Important in the
    // case of component auctions.
    const raw_ptr<InterestGroupAuction> auction;
  };

  // Combines a Bid with seller score and seller state needed to invoke its
  // ReportResult() method.
  struct ScoredBid {
    ScoredBid(double score,
              absl::optional<uint32_t> scoring_signals_data_version,
              std::unique_ptr<Bid> bid,
              auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
                  component_auction_modified_bid_params);
    ~ScoredBid();

    // The seller's desirability score for the bid.
    const double score;

    // The seller's scoring signals version.
    const absl::optional<uint32_t> scoring_signals_data_version;

    // The bid that came from the bidder or component Auction.
    const std::unique_ptr<Bid> bid;

    // Modifications that should be applied to `bid` before the parent
    // auction uses it. Only present for bids in component Auctions. When
    // the top-level auction creates a ScoredBid represending the result from
    // a component auction, the params have already been applied to the
    // underlying Bid, so the params are no longer needed.
    const auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params;
  };

  // Callback that's called when a phase of the InterestGroupAuction completes.
  // Always invoked asynchronously.
  using AuctionPhaseCompletionCallback = base::OnceCallback<void(bool success)>;

  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  // All passed in raw pointers must remain valid until the InterestGroupAuction
  // is destroyed. `config` is typically owned by the AuctionRunner's
  // `owned_auction_config_` field. `parent` should be the parent
  // InterestGroupAuction if this is a component auction, and null, otherwise.
  InterestGroupAuction(const blink::AuctionConfig* config,
                       const InterestGroupAuction* parent,
                       AuctionWorkletManager* auction_worklet_manager,
                       InterestGroupManagerImpl* interest_group_manager,
                       base::Time auction_start_time);

  InterestGroupAuction(const InterestGroupAuction&) = delete;
  InterestGroupAuction& operator=(const InterestGroupAuction&) = delete;

  ~InterestGroupAuction() override;

  // Starts loading the interest groups that can participate in an auction.
  //
  // Both seller and buyer origins are filtered by
  // `is_interest_group_api_allowed`, and any any not allowed to use the API
  // are excluded from participating in the auction.
  //
  // Invokes `load_interest_groups_phase_callback` asynchronously on
  // completion. Passes it false if there are no interest groups that may
  // participate in the auction (possibly because sellers aren't allowed to
  // participate in the auction)
  void StartLoadInterestGroupsPhase(
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
      AuctionPhaseCompletionCallback load_interest_groups_phase_callback);

  // Starts bidding and scoring phase of the auction.
  //
  // `on_seller_receiver_callback`, if non-null, is invoked once the seller
  // worklet has been received, or if the seller worklet is no longer needed
  // (e.g., if all bidders fail to bid before the seller worklet has
  // been received). This is needed so that in the case of component auctions,
  // the top-level seller worklet will only be requested once all component
  // seller worklets have been received, to prevent deadlock (the top-level
  // auction could be waiting on a bid from a seller, while the top-level
  // seller worklet being is blocking a component seller worklet from being
  // created, due to the process limit). Unlike other callbacks,
  // `on_seller_receiver_callback` may be called synchronously.
  //
  // `bidding_and_scoring_phase_callback` is invoked asynchronously when
  // either the auction has failed to produce a winner, or the auction has a
  // winner. `success` is true only when there is a winner.
  void StartBiddingAndScoringPhase(
      base::OnceClosure on_seller_receiver_callback,
      AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback);

  // Starts the reporting phase of the auction. Callback is invoked
  // asynchronously when either the auction has encountered a fatal error, or
  // when all reporting URLs (if any) have been retrieved from the applicable
  // worklets. `success` is true if the final status of the auction is
  // `kSuccess`.
  //
  // If this is a component auction, `top_seller_signals` must populated and
  // be the output from the top-level seller's reportResult() method.
  void StartReportingPhase(
      absl::optional<std::string> top_seller_signals,
      AuctionPhaseCompletionCallback reporting_phase_callback);

  // Close all Mojo pipes and release all weak pointers. Called when an
  // auction fails and on auction complete.
  void ClosePipes();

  // Returns the number of interest groups participating in the auction that can
  // potentially make bids. Includes interest groups in component auctions.
  // Double-counts interest groups participating multiple times in different
  // InterestGroupAuctions.
  size_t NumPotentialBidders() const;

  // Returns all interest groups that bid in an auction. Expected to be called
  // after the bidding and scoring phase completes, but before the reporting
  // phase. Returns an empty set if the auction failed for any reason other
  // than the seller rejecting all bids.
  //
  // TODO(mmenke): Consider calling this after the reporting phase.
  void GetInterestGroupsThatBid(blink::InterestGroupSet& interest_groups) const;

  // Retrieves any debug reporting URLs. May only be called once, since it
  // takes ownership of stored reporting URLs.
  void TakeDebugReportUrls(std::vector<GURL>& debug_win_report_urls,
                           std::vector<GURL>& debug_loss_report_urls);

  // Retrieves the ad beacon map. May only be called once, since it takes
  // ownership of the stored ad beacon map.
  ReportingMetadata TakeAdBeaconMap() { return std::move(ad_beacon_map_); }

  // Retrieves any reporting URLs returned by ReportWin() and ReportResult()
  // methods. May only be called after an auction has completed successfully.
  // May only be called once, since it takes ownership of stored reporting
  // URLs.
  std::vector<GURL> TakeReportUrls();

  // Retrieves all requests to the Private Aggregation API returned by
  // GenerateBid(), ScoreAd(), ReportWin() and ReportResult(). The return value
  // is keyed by reporting origin of the associated requests. May only be called
  // after an auction has completed (successfully or not). May only be called
  // once, since it takes ownership of stored reporting URLs.
  std::map<url::Origin, PrivateAggregationRequests>
  TakePrivateAggregationRequests();

  // Retrieves any errors from the auction. May only be called once, since it
  // takes ownership of stored errors.
  std::vector<std::string> TakeErrors();

  // Retrieves (by appending) all owners of interest groups that participated
  // in this auction (or any of its child auctions) that successfully loaded
  // at least one interest group. May only be called after the auction has
  // completed, for either success or failure. Duplication is possible,
  // particularly if an owner is listed in multiple auction components. May
  // only be called once, since it moves the stored origins.
  void TakePostAuctionUpdateOwners(std::vector<url::Origin>& owners);

  // Returns the top bid of the auction. May only be invoked after the
  // bidding and scoring phase has completed successfully.
  ScoredBid* top_bid();

 private:
  using AuctionList = std::list<std::unique_ptr<InterestGroupAuction>>;

  // BuyerHelpers create and own the BidStates for a particular buyer, to
  // better handle per-buyer cross-interest-group behavior (e.g., enforcing
  // a shared per-buyer timeout, only generating bids for the highest priority N
  // interest groups of a particular buyer, etc).
  class BuyerHelper;

  // ---------------------------------
  // Load interest group phase methods
  // ---------------------------------

  // Invoked whenever the interest groups for a buyer have loaded. Adds
  // `interest_groups` to `bid_states_`.
  void OnInterestGroupRead(std::vector<StorageInterestGroup> interest_groups);

  // Invoked when the interest groups for an entire component auction have
  // loaded. If `success` is false, removes the component auction.
  void OnComponentInterestGroupsRead(AuctionList::iterator component_auction,
                                     bool success);

  // Invoked when the interest groups for a buyer or for an entire component
  // auction have loaded. Completes the loading phase if no pending loads
  // remain.
  void OnOneLoadCompleted();

  // Invoked once the interest group load phase has completed. Never called
  // synchronously from StartLoadInterestGroupsPhase(), to avoid reentrancy
  // (AuctionRunner::callback_ cannot be invoked until
  // AuctionRunner::CreateAndStart() completes). `auction_result` is the
  // result of trying to load the interest groups that can participate in the
  // auction. It's AuctionResult::kSuccess if there are interest groups that
  // can take part in the auction, and a failure value otherwise.
  void OnStartLoadInterestGroupsPhaseComplete(AuctionResult auction_result);

  // -------------------------------------
  // Generate and score bids phase methods
  // -------------------------------------

  // Called when a component auction has received a worklet. Calls
  // RequestSellerWorklet() if all component auctions have received worklets.
  // See StartBiddingAndScoringPhase() for discussion of this.
  void OnComponentSellerWorkletReceived();

  // Requests a seller worklet from the AuctionWorkletManager.
  void RequestSellerWorklet();

  // Called when RequestSellerWorklet() returns. Starts scoring bids, if there
  // are any.
  void OnSellerWorkletReceived();

  // Invoked by the AuctionWorkletManager on fatal errors, at any point after
  // a SellerWorklet has been provided. Results in auction immediately
  // failing. Unlike most other methods, may be invoked during either the
  // generate bid phase or the reporting phase, since the seller worklet is
  // not unloaded between the two phases.
  void OnSellerWorkletFatalError(
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // True if all bids have been generated and scored.
  bool AllBidsScored() const {
    return outstanding_bid_sources_ == 0 && bids_being_scored_ == 0 &&
           unscored_bids_.empty();
  }

  // Invoked when a component auction completes. If `success` is true, gets
  // the Bid from `component_auction` and passes a copy of it to ScoreBid().
  void OnComponentAuctionComplete(InterestGroupAuction* component_auction,
                                  bool success);

  // Called when a potential source of bids has finished generating bids. This
  // could be either a component auction completing (with or without generating
  // a bid) or a BuyerHelper that has finished generating bids. Must be called
  // only after ScoreBidIfReady() has been called for all bids the bidder
  // generated.
  //
  // Updates `outstanding_bid_sources_`, flushes pending scoring signals
  // requests, and advances to the next state of the auction, if the bidding and
  // scoring phase is complete.
  void OnBidSourceDone();

  // Calls into the seller asynchronously to score the passed in bid.
  void ScoreBidIfReady(std::unique_ptr<Bid> bid);

  // Validates the passed in result from ScoreBidComplete(). On failure, reports
  // a bad message to the active receiver in `score_ad_receivers_` and returns
  // false.
  bool ValidateScoreBidCompleteResult(
      double score,
      auction_worklet::mojom::ComponentAuctionModifiedBidParams*
          component_auction_modified_bid_params,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url);

  // auction_worklet::mojom::ScoreAdClient implementation:
  void OnScoreAdComplete(
      double score,
      auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
          component_auction_modified_bid_params,
      uint32_t scoring_signals_data_version,
      bool has_scoring_signals_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      const std::vector<std::string>& errors) override;

  // Invoked when the bid becomes the new highest scoring other bid, to handle
  // calculation of post auction signals. `owner` is nullptr in the event the
  // bid is tied with the top bid, and they have different origins.
  void OnNewHighestScoringOtherBid(double score,
                                   double bid_value,
                                   const url::Origin* owner);

  absl::optional<std::string> PerBuyerSignals(const BidState* state);
  absl::optional<base::TimeDelta> PerBuyerTimeout(const BidState* state);
  absl::optional<base::TimeDelta> SellerTimeout();

  // If AllBidsScored() is true, completes the bidding and scoring phase.
  void MaybeCompleteBiddingAndScoringPhase();

  // Invoked when the bidding and scoring phase of an auction completes.
  // `auction_result` is AuctionResult::kSuccess if the auction has a winner,
  // and some other value otherwise. Appends `errors` to `errors_`.
  void OnBiddingAndScoringComplete(AuctionResult auction_result,
                                   const std::vector<std::string>& errors = {});

  // -----------------------
  // Reporting phase methods
  // -----------------------

  // Sequence of asynchronous methods to call into the seller and then bidder
  // worklet to report a win. Will ultimately invoke
  // `reporting_phase_callback_`, which will delete the auction.
  void ReportSellerResult(absl::optional<std::string> top_seller_signals);
  void OnReportSellerResultComplete(
      const absl::optional<std::string>& signals_for_winner,
      const absl::optional<GURL>& seller_report_url,
      const base::flat_map<std::string, GURL>& seller_ad_beacon_map,
      PrivateAggregationRequests pa_requests,
      const std::vector<std::string>& error_msgs);
  void LoadBidderWorkletToReportBidWin(const std::string& signals_for_winner);
  void ReportBidWin(const std::string& signals_for_winner);
  void OnReportBidWinComplete(
      const absl::optional<GURL>& bidder_report_url,
      const base::flat_map<std::string, GURL>& bidder_ad_beacon_map,
      PrivateAggregationRequests pa_requests,
      const std::vector<std::string>& error_msgs);

  // Called when the component SellerWorklet with the bidder that won an
  // auction has an out-of-band fatal error during the ReportResult() call.
  void OnWinningComponentSellerWorkletFatalError(
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Called when the BidderWorklet that won an auction has an out-of-band
  // fatal error during the ReportWin() call.
  void OnWinningBidderWorkletFatalError(
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Invoked when the nested component auction with the winning bid's
  // reporting phase is complete. Completes the reporting phase for `this`.
  void OnComponentAuctionReportingPhaseComplete(bool success);

  // Called when the final phase of the auction completes. Unconditionally
  // sets `final_auction_result`, even if `auction_result` is
  // AuctionResult::kSuccess, unlike other phase completion methods. Appends
  // `errors` to `errors_`.
  void OnReportingPhaseComplete(AuctionResult auction_result,
                                const std::vector<std::string>& errors = {});

  // -----------------------------------
  // Methods not associated with a phase
  // -----------------------------------

  // Creates a ComponentAuctionOtherSeller to pass to SellerWorklets when
  // dealing with `bid`. If `this` is a component auction, returns an object
  // with a `top_level_seller`. If this is a top-level auction and `bid` comes
  // from a component auction, returns an object with a `component_seller` to
  // `bid's` seller.
  auction_worklet::mojom::ComponentAuctionOtherSellerPtr GetOtherSellerParam(
      const Bid& bid) const;

  // Requests a WorkletHandle for the interest group identified by
  // `bid_state`, using the provided callbacks. Returns true if a worklet was
  // received synchronously.
  [[nodiscard]] bool RequestBidderWorklet(
      BidState& bid_state,
      base::OnceClosure worklet_available_callback,
      AuctionWorkletManager::FatalErrorCallback fatal_error_callback);

  // Replace `${}` placeholders in debug report URLs for post auction signals
  // if exist.
  static GURL FillPostAuctionSignals(const GURL& url,
                                     const PostAuctionSignals& signals,
                                     const absl::optional<PostAuctionSignals>&
                                         top_level_signals = absl::nullopt);

  // Tracing ID associated with the Auction. A nestable
  // async "Auction" trace
  // event lasts for the lifetime of `this`. Sequential events that apply to
  // the entire auction are logged using this ID, including potentially
  // out-of-process events by bidder and seller worklet reporting methods.
  const uint64_t trace_id_;

  const raw_ptr<AuctionWorkletManager> auction_worklet_manager_;
  const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  // Configuration of this auction.
  raw_ptr<const blink::AuctionConfig> config_;
  // If this is a component auction, the parent Auction. Null, otherwise.
  const raw_ptr<const InterestGroupAuction> parent_;

  // Component auctions that are part of this auction. This auction manages
  // their state transition, and their bids may participate in this auction as
  // well. Component auctions that fail in the load phase are removed from
  // this list, to avoid trying to load their worklets during the scoring
  // phase.
  AuctionList component_auctions_;

  // Final result of the auction, once completed. Null before completion.
  absl::optional<AuctionResult> final_auction_result_;

  // Each phases uses its own callback, to make sure that the right callback
  // is invoked when the phase completes.
  AuctionPhaseCompletionCallback load_interest_groups_phase_callback_;
  AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback_;
  AuctionPhaseCompletionCallback reporting_phase_callback_;

  // Invoked in the bidding and scoring phase, once the seller worklet has
  // loaded. May be null.
  base::OnceClosure on_seller_receiver_callback_;

  // The number of buyers and component auctions with pending interest group
  // loads from storage. Decremented each time either the interest groups for
  // a buyer or all buyers for a component are read.
  // `load_interest_groups_phase_callback` is invoked once this hits 0.
  size_t num_pending_loads_ = 0;

  // True once a seller worklet has been received from the
  // AuctionWorkletManager.
  bool seller_worklet_received_ = false;

  // Number of bidders that are still attempting to generate bids. This includes
  // both BuyerHelpers and component auctions. BuyerHelpers may generate
  // multiple bids (or no bids).
  //
  // When this reaches 0, the SellerWorklet's SendPendingSignalsRequests()
  // method should be invoked, so it can send any pending scoring signals
  // requests.
  int outstanding_bid_sources_ = 0;

  // Number of bids that have been send to the seller worklet to score, but
  // that haven't yet had their score received from the seller worklet.
  int bids_being_scored_ = 0;

  // The number of `component_auctions_` that have yet to request seller
  // worklets. Once it hits 0, the seller worklet for `this` is loaded. See
  // StartBiddingAndScoringPhase() for more details.
  size_t pending_component_seller_worklet_requests_ = 0;

  bool any_bid_made_ = false;

  // State of all buyers participating in the auction. Excludes buyers that
  // don't own any interest groups the user belongs to.
  std::vector<std::unique_ptr<BuyerHelper>> buyer_helpers_;

  // Bids waiting on the seller worklet to load before scoring. Does not
  // include bids that are currently waiting on the worklet's ScoreAd() method
  // to complete.
  std::vector<std::unique_ptr<Bid>> unscored_bids_;

  // The time the auction started. Use a single base time for all Worklets, to
  // present a more consistent view of the universe.
  const base::Time auction_start_time_;

  // The number of buyers in the AuctionConfig that passed the
  // IsInterestGroupApiAllowedCallback filter and interest groups were found
  // for. Includes buyers from nested component auctions. Double-counts buyers
  // in multiple Auctions.
  int num_owners_loaded_ = 0;

  // The number of buyers with InterestGroups participating in an auction.
  // Includes buyers from nested component auctions. Double-counts buyers in
  // multiple Auctions.
  int num_owners_with_interest_groups_ = 0;

  // A list of all buyer owners that participated in this auction and had at
  // least one interest group. These owners will have their interest groups
  // updated after a successful auction, barring rate-limiting.
  std::vector<url::Origin> post_auction_update_owners_;

  // A list of all interest groups that need to have their priority adjusted.
  // The new rates will be committed after a successful auction.
  std::vector<std::pair<blink::InterestGroupKey, double>>
      post_auction_priority_updates_;

  // The highest scoring bid so far. Null if no bid has been accepted yet.
  std::unique_ptr<ScoredBid> top_bid_;
  // Number of bidders with the same score as `top_bidder`.
  size_t num_top_bids_ = 0;
  // Number of bidders with the same score as `second_highest_score_`. If the
  // second highest score matches the highest score, this does not include the
  // top bid.
  size_t num_second_highest_bids_ = 0;

  // The numeric value of the bid that got the second highest score. When
  // there's a tie for the second highest score, one of the second highest
  // scoring bids is randomly chosen.
  double highest_scoring_other_bid_ = 0.0;
  double second_highest_score_ = 0.0;
  // Whether all bids of the highest score are from the same interest group
  // owner.
  bool at_most_one_top_bid_owner_ = true;
  // Will be null in the end if there are interest groups having the second
  // highest score with different owners. That includes the top bid itself, in
  // the case there's a tie for the top bid.
  absl::optional<url::Origin> highest_scoring_other_bid_owner_;

  // Holds a reference to the SellerWorklet used by the auction.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> seller_worklet_handle_;

  // Report URLs from reportResult() and reportWin() methods. An auction's
  // report URL from reportResult() comes before the URL from its reportWin()
  // method if there is one. Returned to `callback_` to deal with, so the
  // auction itself can be deleted at the end of the auction.
  std::vector<GURL> report_urls_;

  // Stores all pending Private Aggregation API report requests until they have
  // been flushed. Keyed by the origin of the script that issued the request
  // (i.e. the reporting origin).
  std::map<url::Origin, PrivateAggregationRequests>
      private_aggregation_requests_;

  // All errors reported by worklets thus far.
  std::vector<std::string> errors_;

  // Ad Beacon URL mapping generated from reportResult() or reportWin() from
  // this auction and its components. Destination is relative to this auction.
  // Returned to `callback_` to deal with, so the Auction itself can be
  // deleted at the end of the auction.
  ReportingMetadata ad_beacon_map_;

  // This is set to true if the scoring phase ran and was able to score all
  // bids that were made (of which there may have been none). This is used to
  // gate accessors that should return nothing if the entire auction failed
  // (e.g., don't want to report bids as having "lost" an auction if the
  // seller failed to load, since neither the bids nor the bidders were the
  // problem).
  bool all_bids_scored_ = false;

  // Receivers for OnScoreAd() callbacks. Owns Bids, which have raw pointers to
  // other objects, so must be last, to avoid triggering tooling to check for
  // dangling pointers.
  mojo::ReceiverSet<auction_worklet::mojom::ScoreAdClient, std::unique_ptr<Bid>>
      score_ad_receivers_;

  base::WeakPtrFactory<InterestGroupAuction> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_H_
