// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/webui_interaction_test_util.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WebUIInteractionTestUtilTest, IsTruthy) {
  EXPECT_FALSE(WebUIInteractionTestUtil::IsTruthy(base::Value()));
  EXPECT_FALSE(WebUIInteractionTestUtil::IsTruthy(base::Value(0)));
  EXPECT_TRUE(WebUIInteractionTestUtil::IsTruthy(base::Value(3)));
  EXPECT_FALSE(WebUIInteractionTestUtil::IsTruthy(base::Value(0.0)));
  EXPECT_TRUE(WebUIInteractionTestUtil::IsTruthy(base::Value(3.0)));
  EXPECT_FALSE(WebUIInteractionTestUtil::IsTruthy(base::Value("")));
  EXPECT_TRUE(WebUIInteractionTestUtil::IsTruthy(base::Value("abc")));
  // Even empty lists and objects/dictionaries are truthy.
  EXPECT_TRUE(
      WebUIInteractionTestUtil::IsTruthy(base::Value(base::Value::List())));
  EXPECT_TRUE(
      WebUIInteractionTestUtil::IsTruthy(base::Value(base::Value::Dict())));
}
