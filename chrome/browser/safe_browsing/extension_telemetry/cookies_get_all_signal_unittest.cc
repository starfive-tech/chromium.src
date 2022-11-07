// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

TEST(CookiesGetAllSignalTest, ConcatFieldsWithAllArgs) {
  CookiesGetAllSignal signal = CookiesGetAllSignal(
      /*extension_id=*/"ext-0",
      /*domain=*/"domain",
      /*name=*/"cookie-1",
      /*path=*/"/path1",
      /*secure=*/true,
      /*store_id=*/"store-1",
      /*url=*/"www.example.com",
      /*is_session=*/true);
  EXPECT_EQ(signal.getUniqueArgSetId(),
            "domaincookie-1/path11store-1www.example.com1");
}

TEST(CookiesGetAllSignalTest, ConcatFieldsWithDefaultArgs) {
  CookiesGetAllSignal signal = CookiesGetAllSignal(
      /*extension_id=*/"",
      /*domain=*/"",
      /*name=*/"",
      /*path=*/"",
      /*secure=*/false,
      /*store_id=*/"",
      /*url=*/"",
      /*is_session=*/false);
  EXPECT_EQ(signal.getUniqueArgSetId(), "00");
}

}  // namespace

}  // namespace safe_browsing
