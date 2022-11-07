// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/private_aggregation_bindings.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

// If returns `absl::nullopt`, will output an error to `error_out`.
absl::optional<absl::uint128> ConvertBigIntToUint128(
    v8::MaybeLocal<v8::BigInt> maybe_bigint,
    std::string* error_out) {
  if (maybe_bigint.IsEmpty()) {
    *error_out = "Failed to interpret as BigInt";
    return absl::nullopt;
  }

  v8::Local<v8::BigInt> local_bigint = maybe_bigint.ToLocalChecked();
  if (local_bigint.IsEmpty()) {
    *error_out = "Failed to interpret as BigInt";
    return absl::nullopt;
  }
  if (local_bigint->WordCount() > 2) {
    *error_out = "BigInt is too large";
    return absl::nullopt;
  }
  // Signals the size of the `words` array to `ToWordsArray()`. The number of
  // elements actually used is then written here by the function.
  int word_count = 2;
  int sign_bit = 0;
  uint64_t words[2];  // Least significant to most significant.
  local_bigint->ToWordsArray(&sign_bit, &word_count, words);
  if (sign_bit) {
    *error_out = "BigInt must be non-negative";
    return absl::nullopt;
  }
  if (word_count == 0) {
    words[0] = 0;
  }
  if (word_count <= 1) {
    words[1] = 0;
  }

  return absl::MakeUint128(words[1], words[0]);
}

// In case of failure, will return `absl::nullopt` and output an error to
// `error_out`.
absl::optional<uint64_t> ParseDebugKey(gin::Dictionary dict,
                                       v8::Local<v8::Context>& context,
                                       std::string* error_out) {
  v8::Local<v8::Value> js_debug_key;

  if (!dict.Get("debug_key", &js_debug_key) || js_debug_key.IsEmpty() ||
      js_debug_key->IsNullOrUndefined()) {
    return absl::nullopt;
  }

  if (js_debug_key->IsUint32()) {
    v8::Maybe<uint32_t> maybe_debug_key = js_debug_key->Uint32Value(context);
    if (maybe_debug_key.IsNothing()) {
      *error_out = "Failed to interpret value as integer";
    }
    return maybe_debug_key.ToChecked();
  }

  if (js_debug_key->IsBigInt()) {
    absl::optional<absl::uint128> maybe_debug_key =
        ConvertBigIntToUint128(js_debug_key->ToBigInt(context), error_out);
    if (!maybe_debug_key.has_value()) {
      return absl::nullopt;
    }
    if (absl::Uint128High64(maybe_debug_key.value()) != 0) {
      *error_out = "BigInt is too large";
      return absl::nullopt;
    }
    return absl::Uint128Low64(maybe_debug_key.value());
  }

  *error_out =
      "debug_key must be either a non-negative integer Number or BigInt";
  return absl::nullopt;
}

}  // namespace

PrivateAggregationBindings::PrivateAggregationBindings(
    AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

PrivateAggregationBindings::~PrivateAggregationBindings() = default;

void PrivateAggregationBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  if (!base::FeatureList::IsEnabled(content::kPrivateAggregationApi)) {
    return;
  }

  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);

  v8::Local<v8::ObjectTemplate> private_aggregation_template =
      v8::ObjectTemplate::New(v8_helper_->isolate());

  v8::Local<v8::FunctionTemplate> send_histogram_report_template =
      v8::FunctionTemplate::New(
          v8_helper_->isolate(),
          &PrivateAggregationBindings::SendHistogramReport, v8_this);
  send_histogram_report_template->RemovePrototype();
  private_aggregation_template->Set(
      v8_helper_->CreateStringFromLiteral("sendHistogramReport"),
      send_histogram_report_template);

  v8::Local<v8::FunctionTemplate> enable_debug_mode_template =
      v8::FunctionTemplate::New(v8_helper_->isolate(),
                                &PrivateAggregationBindings::EnableDebugMode,
                                v8_this);
  enable_debug_mode_template->RemovePrototype();
  private_aggregation_template->Set(
      v8_helper_->CreateStringFromLiteral("enableDebugMode"),
      enable_debug_mode_template);

  global_template->Set(
      v8_helper_->CreateStringFromLiteral("privateAggregation"),
      private_aggregation_template);
}

void PrivateAggregationBindings::Reset() {
  private_aggregation_contributions_.clear();
  debug_mode_details_.is_enabled = false;
  debug_mode_details_.debug_key = nullptr;
}

std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
PrivateAggregationBindings::TakePrivateAggregationRequests() {
  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> requests;

  requests.reserve(private_aggregation_contributions_.size());
  base::ranges::transform(
      private_aggregation_contributions_, std::back_inserter(requests),
      [this](content::mojom::AggregatableReportHistogramContributionPtr&
                 contribution) {
        return auction_worklet::mojom::PrivateAggregationRequest::New(
            std::move(contribution),
            // TODO(alexmt): consider allowing this to be set
            content::mojom::AggregationServiceMode::kDefault,
            debug_mode_details_.Clone());
      });
  private_aggregation_contributions_.clear();

  return requests;
}

void PrivateAggregationBindings::SendHistogramReport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  // Any additional arguments are ignored.
  if (args.Length() == 0 || args[0].IsEmpty() || !args[0]->IsObject()) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendHistogramReport requires 1 object parameter")));
    return;
  }

  gin::Dictionary dict(isolate);

  if (!gin::ConvertFromV8(isolate, args[0], &dict)) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "Invalid argument in sendHistogramReport")));
    return;
  }

  v8::Local<v8::Value> js_bucket;
  v8::Local<v8::Value> js_value;

  if (!dict.Get("bucket", &js_bucket) || js_bucket.IsEmpty() ||
      js_bucket->IsNullOrUndefined()) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "Invalid or missing bucket in sendHistogramReport argument")));
    return;
  }

  if (!dict.Get("value", &js_value) || js_value.IsEmpty() ||
      js_value->IsNullOrUndefined()) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "Invalid or missing value in sendHistogramReport argument")));
    return;
  }

  absl::uint128 bucket;
  int value;

  if (js_bucket->IsUint32()) {
    v8::Maybe<uint32_t> maybe_bucket = js_bucket->Uint32Value(context);
    if (maybe_bucket.IsNothing()) {
      isolate->ThrowException(
          v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
              "Failed to interpret value as integer")));
      return;
    }
    bucket = maybe_bucket.ToChecked();
  } else if (js_bucket->IsBigInt()) {
    std::string error;
    absl::optional<absl::uint128> maybe_bucket =
        ConvertBigIntToUint128(js_bucket->ToBigInt(context), &error);
    if (!maybe_bucket.has_value()) {
      DCHECK(base::IsStringUTF8(error));
      isolate->ThrowException(v8::Exception::TypeError(
          v8_helper->CreateUtf8String(error).ToLocalChecked()));
      return;
    }
    bucket = maybe_bucket.value();
  } else {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "Bucket must be either an integer Number or BigInt")));
    return;
  }

  if (js_value->IsInt32()) {
    v8::Maybe<int32_t> maybe_value = js_value->Int32Value(context);
    if (maybe_value.IsNothing()) {
      isolate->ThrowException(
          v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
              "Failed to interpret value as integer")));
      return;
    }
    value = maybe_value.ToChecked();
  } else if (js_value->IsBigInt()) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateStringFromLiteral("Value cannot be a BigInt")));
    return;
  } else {
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateStringFromLiteral("Value must be an integer Number")));
    return;
  }

  if (value < 0) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateStringFromLiteral("Value must be non-negative")));
    return;
  }

  bindings->private_aggregation_contributions_.push_back(
      content::mojom::AggregatableReportHistogramContribution::New(bucket,
                                                                   value));
}

void PrivateAggregationBindings::EnableDebugMode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  if (bindings->debug_mode_details_.is_enabled) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "enableDebugMode may be called at most once")));
    return;
  }

  // If no arguments are provided, no debug key is set.
  if (args.Length() >= 1 && !args[0].IsEmpty()) {
    gin::Dictionary dict(isolate);

    if (!gin::ConvertFromV8(isolate, args[0], &dict)) {
      isolate->ThrowException(
          v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
              "Invalid argument in enableDebugMode")));
      return;
    }

    std::string error;
    absl::optional<uint64_t> maybe_debug_key =
        ParseDebugKey(dict, context, &error);
    if (!maybe_debug_key.has_value()) {
      DCHECK(base::IsStringUTF8(error));
      isolate->ThrowException(v8::Exception::TypeError(
          v8_helper->CreateUtf8String(error).ToLocalChecked()));
      return;
    }

    bindings->debug_mode_details_.debug_key =
        content::mojom::DebugKey::New(maybe_debug_key.value());
  }

  bindings->debug_mode_details_.is_enabled = true;
}

}  // namespace auction_worklet
