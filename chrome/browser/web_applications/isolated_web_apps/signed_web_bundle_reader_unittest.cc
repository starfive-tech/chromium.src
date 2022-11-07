// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/signed_web_bundle_utils.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

namespace {

constexpr uint8_t kEd25519PublicKey[32] = {0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0,
                                           0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0,
                                           0, 0, 0, 0, 2, 2, 2, 0, 0, 0};

constexpr uint8_t kEd25519Signature[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0};

}  // namespace

class SignedWebBundleReaderTest : public testing::Test {
 protected:
  void SetUp() override {
    parser_factory_ =
        std::make_unique<web_package::MockWebBundleParserFactory>();

    response_ = web_package::mojom::BundleResponse::New();
    response_->response_code = 200;
    response_->payload_offset = 0;
    response_->payload_length = sizeof(kResponseBody) - 1;

    GURL primary_url("isolated-app://foo");

    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> items;
    items.insert({primary_url,
                  web_package::mojom::BundleResponseLocation::New(
                      response_->payload_offset, response_->payload_length)});
    metadata_ = web_package::mojom::BundleMetadata::New();
    metadata_->primary_url = primary_url;
    metadata_->requests = std::move(items);

    web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr
        signature_stack_entry =
            web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
    signature_stack_entry->public_key =
        std::vector(std::begin(kEd25519PublicKey), std::end(kEd25519PublicKey));
    signature_stack_entry->signature =
        std::vector(std::begin(kEd25519Signature), std::end(kEd25519Signature));

    std::vector<web_package::mojom::BundleIntegrityBlockSignatureStackEntryPtr>
        signature_stack;
    signature_stack.push_back(std::move(signature_stack_entry));

    integrity_block_ = web_package::mojom::BundleIntegrityBlock::New();
    // TODO(crbug.com/1315947): Provide proper integrity block data here once
    // integrity verification is implemented.
    integrity_block_->size = 123;
    integrity_block_->signature_stack = std::move(signature_stack);
  }

  void TearDown() override {
    // Allow cleanup tasks posted by the destructor of `web_package::SharedFile`
    // to run.
    task_environment_.RunUntilIdle();
  }

  using VerificationAction = SignedWebBundleReader::IntegrityVerificationAction;

  std::unique_ptr<SignedWebBundleReader> CreateReaderAndInitialize(
      SignedWebBundleReader::ReadErrorCallback callback,
      VerificationAction verification_action =
          VerificationAction::ContinueAndVerifyIntegrity(),
      const std::string test_file_data = kResponseBody) {
    // Provide a buffer that contains the contents of just a single
    // response. We do not need to provide an integrity block or metadata
    // here, since reading them is completely mocked. Only response bodies
    // are read from an actual (temporary) file.
    base::FilePath temp_file_path;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path));
    EXPECT_EQ(test_file_data.size(), static_cast<size_t>(base::WriteFile(
                                         temp_file_path, test_file_data.data(),
                                         test_file_data.size())));

    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(parser_factory_.get())));

    return SignedWebBundleReader::CreateAndStartReading(
        temp_file_path,
        base::BindLambdaForTesting(
            [verification_action](
                const std::vector<web_package::Ed25519PublicKey>&
                    public_key_stack,
                base::OnceCallback<void(VerificationAction)> callback) {
              EXPECT_EQ(public_key_stack.size(), 1ul);
              EXPECT_TRUE(base::ranges::equal(public_key_stack[0].bytes(),
                                              kEd25519PublicKey));

              std::move(callback).Run(verification_action);
            }),
        std::move(callback));
  }

  base::expected<web_package::mojom::BundleResponsePtr,
                 SignedWebBundleReader::ReadResponseError>
  ReadAndFulfillResponse(
      SignedWebBundleReader& reader,
      const network::ResourceRequest& resource_request,
      web_package::mojom::BundleResponseLocationPtr expected_read_response_args,
      web_package::mojom::BundleResponsePtr response,
      web_package::mojom::BundleResponseParseErrorPtr error = nullptr) {
    base::test::TestFuture<
        base::expected<web_package::mojom::BundleResponsePtr,
                       SignedWebBundleReader::ReadResponseError>>
        response_result;
    reader.ReadResponse(resource_request, response_result.GetCallback());

    parser_factory_->RunResponseCallback(std::move(expected_read_response_args),
                                         std::move(response), std::move(error));

    return response_result.Take();
  }

  void SimulateAndWaitForParserDisconnect(SignedWebBundleReader& reader) {
    base::RunLoop run_loop;
    reader.SetParserDisconnectCallbackForTesting(run_loop.QuitClosure());
    parser_factory_->SimulateParserDisconnect();
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<web_package::MockWebBundleParserFactory> parser_factory_;
  web_package::mojom::BundleIntegrityBlockPtr integrity_block_;
  web_package::mojom::BundleMetadataPtr metadata_;

  constexpr static char kResponseBody[] = "test";
  web_package::mojom::BundleResponsePtr response_;
};

TEST_F(SignedWebBundleReaderTest, ReadValidIntegrityBlockAndMetadata) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  EXPECT_EQ(reader->GetPrimaryURL(), metadata_->primary_url);
  EXPECT_EQ(reader->GetEntries().size(), 1ul);
  EXPECT_EQ(reader->GetEntries()[0], metadata_->primary_url);
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockError) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(
      nullptr, web_package::mojom::BundleIntegrityBlockParseError::New());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<
          web_package::mojom::BundleIntegrityBlockParseErrorPtr>(*parse_error));
}

TEST_F(SignedWebBundleReaderTest, ReadInvalidIntegrityBlockSize) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  web_package::mojom::BundleIntegrityBlockPtr integrity_block =
      web_package::mojom::BundleIntegrityBlock::New();
  integrity_block->size = 0;
  parser_factory_->RunIntegrityBlockCallback(std::move(integrity_block));

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<
          web_package::mojom::BundleIntegrityBlockParseErrorPtr>(*parse_error));
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockWithParserCrash) {
  parser_factory_->SimulateParseIntegrityBlockCrash();
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<
          web_package::mojom::BundleIntegrityBlockParseErrorPtr>(*parse_error));
  EXPECT_EQ(absl::get<web_package::mojom::BundleIntegrityBlockParseErrorPtr>(
                *parse_error)
                ->type,
            web_package::mojom::BundleParseErrorType::kParserInternalError);
}

TEST_F(SignedWebBundleReaderTest, ReadIntegrityBlockAndAbort) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader =
      CreateReaderAndInitialize(parse_error_future.GetCallback(),
                                VerificationAction::Abort("test error"));

  parser_factory_->RunIntegrityBlockCallback(integrity_block_.Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  ASSERT_TRUE(parse_error.has_value());

  auto* error =
      absl::get_if<SignedWebBundleReader::AbortedByCaller>(&*parse_error);
  ASSERT_TRUE(error);
  EXPECT_EQ(error->message, "test error");
}

// TODO(crbug.com/1315947): Test integrity block signature verification.
// TODO(crbug.com/1315947): Test skipping of  signature verification on
// ChromeOS.

TEST_F(SignedWebBundleReaderTest, ReadMetadataError) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(
      integrity_block_->size, nullptr,
      web_package::mojom::BundleMetadataParseError::New());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<web_package::mojom::BundleMetadataParseErrorPtr>(
          *parse_error));
}

TEST_F(SignedWebBundleReaderTest, ReadMetadataWithParserCrash) {
  parser_factory_->SimulateParseMetadataCrash();
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_TRUE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kError);
  EXPECT_TRUE(
      absl::holds_alternative<web_package::mojom::BundleMetadataParseErrorPtr>(
          *parse_error));
  EXPECT_EQ(
      absl::get<web_package::mojom::BundleMetadataParseErrorPtr>(*parse_error)
          ->type,
      web_package::mojom::BundleParseErrorType::kParserInternalError);
}

TEST_F(SignedWebBundleReaderTest, ReadResponse) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = metadata_->primary_url;

  auto response = ReadAndFulfillResponse(
      *reader.get(), resource_request,
      metadata_->requests[metadata_->primary_url]->Clone(), response_->Clone());
  EXPECT_TRUE(response.has_value()) << response.error().message;
  EXPECT_EQ((*response)->response_code, 200);
  EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
  EXPECT_EQ((*response)->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithFragment) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetRefStr("baz");
  resource_request.url = metadata_->primary_url.ReplaceComponents(replacements);

  auto response = ReadAndFulfillResponse(
      *reader.get(), resource_request,
      metadata_->requests[metadata_->primary_url]->Clone(), response_->Clone());
  EXPECT_TRUE(response.has_value()) << response.error().message;
  EXPECT_EQ((*response)->response_code, 200);
  EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
  EXPECT_EQ((*response)->payload_length, response_->payload_length);
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithPath) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetPathStr("/foo");
  resource_request.url = metadata_->primary_url.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(response.error().message,
            "The Web Bundle does not contain a response for the provided URL: "
            "isolated-app://foo/foo");
}

TEST_F(SignedWebBundleReaderTest, ReadNonExistingResponseWithQuery) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  GURL::Replacements replacements;
  replacements.SetQueryStr("foo");
  resource_request.url = metadata_->primary_url.ReplaceComponents(replacements);

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kResponseNotFound);
  EXPECT_EQ(response.error().message,
            "The Web Bundle does not contain a response for the provided URL: "
            "isolated-app://foo/?foo");
}

TEST_F(SignedWebBundleReaderTest, ReadResponseError) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = metadata_->primary_url;

  auto response = ReadAndFulfillResponse(
      *reader.get(), resource_request,
      metadata_->requests[metadata_->primary_url]->Clone(), nullptr,
      web_package::mojom::BundleResponseParseError::New(
          web_package::mojom::BundleParseErrorType::kFormatError, "test"));
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(response.error().type,
            SignedWebBundleReader::ReadResponseError::Type::kFormatError);
  EXPECT_EQ(response.error().message, "test");
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserDisconnect) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = metadata_->primary_url;

  SimulateAndWaitForParserDisconnect(*reader.get());
  {
    auto response = ReadAndFulfillResponse(
        *reader.get(), resource_request,
        metadata_->requests[metadata_->primary_url]->Clone(),
        response_->Clone());
    EXPECT_TRUE(response.has_value()) << response.error().message;
    EXPECT_EQ((*response)->response_code, 200);
    EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
    EXPECT_EQ((*response)->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 2);

  // Simulate another disconnect to verify that the reader can recover from
  // multiple disconnects over the course of its lifetime.
  SimulateAndWaitForParserDisconnect(*reader.get());
  {
    auto response = ReadAndFulfillResponse(
        *reader.get(), resource_request,
        metadata_->requests[metadata_->primary_url]->Clone(),
        response_->Clone());
    EXPECT_TRUE(response.has_value()) << response.error().message;
    EXPECT_EQ((*response)->response_code, 200);
    EXPECT_EQ((*response)->payload_offset, response_->payload_offset);
    EXPECT_EQ((*response)->payload_length, response_->payload_length);
  }

  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 3);
}

TEST_F(SignedWebBundleReaderTest,
       SimulateParserDisconnectWithFileErrorWhenReconnecting) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  SimulateAndWaitForParserDisconnect(*reader.get());
  reader->SetReconnectionFileErrorForTesting(
      base::File::Error::FILE_ERROR_ACCESS_DENIED);

  network::ResourceRequest resource_request;
  resource_request.url = metadata_->primary_url;

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());
  auto response = response_result.Take();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(
      response.error().type,
      SignedWebBundleReader::ReadResponseError::Type::kParserInternalError);
  EXPECT_EQ(response.error().message,
            "Unable to open file: FILE_ERROR_ACCESS_DENIED");
  EXPECT_EQ(parser_factory_->GetParserCreationCount(), 1);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseWithParserCrash) {
  parser_factory_->SimulateParseResponseCrash();
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = metadata_->primary_url;

  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      response_result;
  reader->ReadResponse(resource_request, response_result.GetCallback());

  auto response = response_result.Take();
  EXPECT_FALSE(response.has_value());
  EXPECT_EQ(
      response.error().type,
      SignedWebBundleReader::ReadResponseError::Type::kParserInternalError);
}

TEST_F(SignedWebBundleReaderTest, ReadResponseBody) {
  base::test::TestFuture<absl::optional<SignedWebBundleReader::ReadError>>
      parse_error_future;
  auto reader = CreateReaderAndInitialize(parse_error_future.GetCallback());

  parser_factory_->RunIntegrityBlockCallback(integrity_block_->Clone());
  parser_factory_->RunMetadataCallback(integrity_block_->size,
                                       metadata_->Clone());

  auto parse_error = parse_error_future.Take();
  EXPECT_FALSE(parse_error.has_value());
  EXPECT_EQ(reader->GetState(), SignedWebBundleReader::State::kInitialized);

  network::ResourceRequest resource_request;
  resource_request.url = metadata_->primary_url;

  auto response = ReadAndFulfillResponse(
      *reader.get(), resource_request,
      metadata_->requests[metadata_->primary_url]->Clone(), response_->Clone());
  EXPECT_TRUE(response.has_value()) << response.error().message;

  std::string response_body =
      ReadAndFulfillResponseBody(*reader.get(), std::move(*response));
  EXPECT_EQ(response_body, kResponseBody);
}

}  // namespace web_app
