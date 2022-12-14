/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow_lite_support/cc/task/processor/bert_preprocessor.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/ascii.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_utils.h"
#include "tensorflow_lite_support/cc/utils/common_utils.h"

namespace tflite {
namespace task {
namespace processor {

using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::support::text::tokenizer::CreateTokenizerFromProcessUnit;
using ::tflite::support::text::tokenizer::TokenizerResult;
using ::tflite::task::core::PopulateTensor;

constexpr int kTokenizerProcessUnitIndex = 0;
constexpr int kIdsTensorIndex = 0;
constexpr int kSegmentIdsTensorIndex = 1;
constexpr int kMaskTensorIndex = 2;
constexpr char kClassificationToken[] = "[CLS]";
constexpr char kSeparator[] = "[SEP]";

/* static */
StatusOr<std::unique_ptr<BertPreprocessor>> BertPreprocessor::Create(
    tflite::task::core::TfLiteEngine* engine,
    const std::initializer_list<int> input_tensor_indices) {
  ASSIGN_OR_RETURN(auto processor, Processor::Create<BertPreprocessor>(
                                       /* num_expected_tensors = */ 3, engine,
                                       input_tensor_indices,
                                       /* requires_metadata = */ false));
  RETURN_IF_ERROR(processor->Init());
  return processor;
}

absl::Status BertPreprocessor::Init() {
  // Try if RegexTokenzier can be found.
  // BertTokenzier is packed in the processing unit SubgraphMetadata.
  const tflite::ProcessUnit* tokenizer_metadata =
      GetMetadataExtractor()->GetInputProcessUnit(kTokenizerProcessUnitIndex);
  ASSIGN_OR_RETURN(tokenizer_, CreateTokenizerFromProcessUnit(
                                   tokenizer_metadata, GetMetadataExtractor()));

  // Sanity check and assign max sequence length.
  if (GetLastDimSize(tensor_indices_[kIdsTensorIndex]) !=
          GetLastDimSize(tensor_indices_[kMaskTensorIndex]) ||
      GetLastDimSize(tensor_indices_[kIdsTensorIndex]) !=
          GetLastDimSize(tensor_indices_[kSegmentIdsTensorIndex])) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "The three input tensors in Bert models are "
            "expected to have same length, but got ids_tensor "
            "(%d), mask_tensor (%d), segment_ids_tensor (%d).",
            GetLastDimSize(tensor_indices_[kIdsTensorIndex]),
            GetLastDimSize(tensor_indices_[kMaskTensorIndex]),
            GetLastDimSize(tensor_indices_[kSegmentIdsTensorIndex])),
        TfLiteSupportStatus::kInvalidNumOutputTensorsError);
  }
  bert_max_seq_len_ = GetLastDimSize(tensor_indices_[kIdsTensorIndex]);

  return absl::OkStatus();
}

absl::Status BertPreprocessor::Preprocess(const std::string& input_text) {
  auto* ids_tensor = GetTensor(kIdsTensorIndex);
  auto* mask_tensor = GetTensor(kMaskTensorIndex);
  auto* segment_ids_tensor = GetTensor(kSegmentIdsTensorIndex);

  std::string processed_input = input_text;
  absl::AsciiStrToLower(&processed_input);

  TokenizerResult input_tokenize_results;
  input_tokenize_results = tokenizer_->Tokenize(processed_input);

  // 2 accounts for [CLS], [SEP]
  absl::Span<const std::string> query_tokens =
      absl::MakeSpan(input_tokenize_results.subwords.data(),
                     input_tokenize_results.subwords.data() +
                         std::min(static_cast<size_t>(bert_max_seq_len_ - 2),
                                  input_tokenize_results.subwords.size()));

  std::vector<std::string> tokens;
  tokens.reserve(2 + query_tokens.size());
  // Start of generating the features.
  tokens.push_back(kClassificationToken);
  // For query input.
  for (const auto& query_token : query_tokens) {
    tokens.push_back(query_token);
  }
  // For Separation.
  tokens.push_back(kSeparator);

  std::vector<int> input_ids(bert_max_seq_len_, 0);
  std::vector<int> input_mask(bert_max_seq_len_, 0);
  // Convert tokens back into ids and set mask
  for (int i = 0; i < tokens.size(); ++i) {
    tokenizer_->LookupId(tokens[i], &input_ids[i]);
    input_mask[i] = 1;
  }
  //                           |<--------bert_max_seq_len_--------->|
  // input_ids                 [CLS] s1  s2...  sn [SEP]  0  0...  0
  // input_masks                 1    1   1...  1    1    0  0...  0
  // segment_ids                 0    0   0...  0    0    0  0...  0

  RETURN_IF_ERROR(PopulateTensor(input_ids, ids_tensor));
  RETURN_IF_ERROR(PopulateTensor(input_mask, mask_tensor));
  RETURN_IF_ERROR(PopulateTensor(std::vector<int>(bert_max_seq_len_, 0),
                                 segment_ids_tensor));
  return absl::OkStatus();
}

int BertPreprocessor::GetLastDimSize(int tensor_index) {
  auto tensor = engine_->GetInput(engine_->interpreter(), tensor_index);
  return tensor->dims->data[tensor->dims->size - 1];
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
