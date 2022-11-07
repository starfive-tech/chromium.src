// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/executable_metadata_service.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/mock_platform_delegate.h"
#include "components/device_signals/core/system_signals/platform_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace device_signals {

namespace {

ExecutableMetadata CreateExecutableMetadata(
    bool is_running,
    const absl::optional<std::string>& public_key_hash) {
  ExecutableMetadata metadata;
  metadata.is_running = is_running;
  metadata.public_key_sha256 = public_key_hash;
  return metadata;
}

}  // namespace

class ExecutableMetadataServiceTest : public testing::Test {
 protected:
  ExecutableMetadataServiceTest() {
    auto mock_platform_delegate = std::make_unique<MockPlatformDelegate>();
    mock_platform_delegate_ = mock_platform_delegate.get();

    executable_metadata_service_ =
        ExecutableMetadataService::Create(std::move(mock_platform_delegate));
  }

  MockPlatformDelegate* mock_platform_delegate_;
  std::unique_ptr<ExecutableMetadataService> executable_metadata_service_;
};

TEST_F(ExecutableMetadataServiceTest, GetAllExecutableMetadata_Empty) {
  FilePathSet empty_set;

  EXPECT_CALL(*mock_platform_delegate_, AreExecutablesRunning(empty_set))
      .WillOnce(Return(FilePathMap<bool>()));

  FilePathMap<ExecutableMetadata> empty_map;
  EXPECT_EQ(executable_metadata_service_->GetAllExecutableMetadata(empty_set),
            empty_map);
}

TEST_F(ExecutableMetadataServiceTest, GetAllExecutableMetadata_Success) {
  base::FilePath running_path =
      base::FilePath::FromUTF8Unsafe("C:\\some\\running\\file\\path.exe");
  base::FilePath not_running_path =
      base::FilePath::FromUTF8Unsafe("C:\\some\\file\\path.exe");

  FilePathSet executable_files;
  executable_files.insert(running_path);
  executable_files.insert(not_running_path);

  FilePathMap<bool> is_running_map;
  is_running_map.insert({running_path, true});
  is_running_map.insert({not_running_path, false});

  EXPECT_CALL(*mock_platform_delegate_, AreExecutablesRunning(executable_files))
      .WillOnce(Return(is_running_map));

  std::string public_key_value = "fake_public_key_value";
  EXPECT_CALL(*mock_platform_delegate_,
              GetSigningCertificatePublicKeyHash(running_path))
      .WillOnce(Return(public_key_value));
  EXPECT_CALL(*mock_platform_delegate_,
              GetSigningCertificatePublicKeyHash(not_running_path))
      .WillOnce(Return(absl::nullopt));

  FilePathMap<ExecutableMetadata> expected_metadata_map;
  expected_metadata_map.insert(
      {running_path, CreateExecutableMetadata(true, public_key_value)});
  expected_metadata_map.insert(
      {not_running_path, CreateExecutableMetadata(false, absl::nullopt)});

  EXPECT_EQ(
      executable_metadata_service_->GetAllExecutableMetadata(executable_files),
      expected_metadata_map);
}

}  // namespace device_signals
