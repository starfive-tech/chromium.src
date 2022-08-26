// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string>

#include "ipcz/ipcz.h"
#include "reference_drivers/sync_reference_driver.h"
#include "test/test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

const IpczDriver& kDefaultDriver = reference_drivers::kSyncReferenceDriver;

using APITest = test::Test;

TEST_F(APITest, CloseInvalid) {
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Close(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr));
}

TEST_F(APITest, CreateNodeInvalid) {
  IpczHandle node;

  // Null driver.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().CreateNode(nullptr, IPCZ_INVALID_DRIVER_HANDLE,
                              IPCZ_NO_FLAGS, nullptr, &node));

  // Null output handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE,
                              IPCZ_NO_FLAGS, nullptr, nullptr));
}

TEST_F(APITest, CreateNode) {
  IpczHandle node;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().CreateNode(&kDefaultDriver, IPCZ_INVALID_DRIVER_HANDLE,
                              IPCZ_NO_FLAGS, nullptr, &node));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Close(node, IPCZ_NO_FLAGS, nullptr));
}

TEST_F(APITest, ConnectNodeInvalid) {
  IpczHandle node = CreateNode(kDefaultDriver);
  IpczDriverHandle transport0, transport1;
  ASSERT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.CreateTransports(
                IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
                IPCZ_NO_FLAGS, nullptr, &transport0, &transport1));

  IpczHandle portal;

  // Invalid node handle
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().ConnectNode(IPCZ_INVALID_HANDLE, transport0, 1,
                               IPCZ_NO_FLAGS, nullptr, &portal));

  // Non-node handle
  auto [a, b] = OpenPortals(node);
  EXPECT_EQ(
      IPCZ_RESULT_INVALID_ARGUMENT,
      ipcz().ConnectNode(a, transport0, 1, IPCZ_NO_FLAGS, nullptr, &portal));
  CloseAll({a, b});

  // Invalid transport
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().ConnectNode(node, IPCZ_INVALID_DRIVER_HANDLE, 1,
                               IPCZ_NO_FLAGS, nullptr, &portal));

  // No initial portals
  EXPECT_EQ(
      IPCZ_RESULT_INVALID_ARGUMENT,
      ipcz().ConnectNode(node, transport0, 0, IPCZ_NO_FLAGS, nullptr, &portal));

  // Null portal storage
  EXPECT_EQ(
      IPCZ_RESULT_INVALID_ARGUMENT,
      ipcz().ConnectNode(node, transport0, 0, IPCZ_NO_FLAGS, nullptr, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.Close(transport0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.Close(transport1, IPCZ_NO_FLAGS, nullptr));

  Close(node);
}

TEST_F(APITest, OpenPortalsInvalid) {
  IpczHandle node = CreateNode(kDefaultDriver);

  IpczHandle a, b;

  // Invalid node.
  EXPECT_EQ(
      IPCZ_RESULT_INVALID_ARGUMENT,
      ipcz().OpenPortals(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, &a, &b));

  // Invalid portal handle(s).
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, nullptr, &b));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &a, nullptr));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr));

  Close(node);
}

TEST_F(APITest, OpenPortals) {
  IpczHandle node = CreateNode(kDefaultDriver);

  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &a, &b));

  CloseAll({a, b, node});
}

TEST_F(APITest, QueryPortalStatusInvalid) {
  IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  // Null portal.
  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS,
                                     nullptr, &status));

  // Not a portal.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(node, IPCZ_NO_FLAGS, nullptr, &status));

  // Null output status.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, nullptr));

  // Invalid status size.
  status.size = 0;
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, &status));

  CloseAll({a, b, node});
}

TEST_F(APITest, QueryPortalStatus) {
  IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(0u, status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);
  EXPECT_EQ(0u, status.flags & IPCZ_PORTAL_STATUS_DEAD);
  EXPECT_EQ(0u, status.num_local_parcels);
  EXPECT_EQ(0u, status.num_local_bytes);
  EXPECT_EQ(0u, status.num_remote_parcels);
  EXPECT_EQ(0u, status.num_remote_bytes);

  Close(b);
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(a, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(IPCZ_PORTAL_STATUS_PEER_CLOSED,
            status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);
  EXPECT_EQ(IPCZ_PORTAL_STATUS_DEAD, status.flags & IPCZ_PORTAL_STATUS_DEAD);

  CloseAll({a, node});
}

TEST_F(APITest, MergePortalsFailure) {
  const IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  // Invalid portal handles.
  EXPECT_EQ(
      IPCZ_RESULT_INVALID_ARGUMENT,
      ipcz().MergePortals(a, IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(
      IPCZ_RESULT_INVALID_ARGUMENT,
      ipcz().MergePortals(IPCZ_INVALID_HANDLE, a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().MergePortals(IPCZ_INVALID_HANDLE, IPCZ_INVALID_HANDLE,
                                IPCZ_NO_FLAGS, nullptr));

  // Can't merge into own peer.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().MergePortals(a, b, IPCZ_NO_FLAGS, nullptr));

  // Can't merge into self.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().MergePortals(a, a, IPCZ_NO_FLAGS, nullptr));

  auto [c, d] = OpenPortals(node);

  // Can't merge a portal that's had parcels put into it.
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, "!"));
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            ipcz().MergePortals(a, c, IPCZ_NO_FLAGS, nullptr));

  // Can't merge a portal that's had parcels retrieved from it.
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, Get(d, &message));
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            ipcz().MergePortals(a, d, IPCZ_NO_FLAGS, nullptr));

  CloseAll({a, b, c, d, node});
}

TEST_F(APITest, MergePortals) {
  const IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);
  auto [c, d] = OpenPortals(node);

  EXPECT_EQ(IPCZ_RESULT_OK, Put(a, "!"));
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().MergePortals(b, c, IPCZ_NO_FLAGS, nullptr));

  // The message from `a` should be routed to `d`, since `b` and `c` have been
  // merged.
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, Get(d, &message));
  EXPECT_EQ("!", message);

  CloseAll({a, d, node});
}

TEST_F(APITest, PutGet) {
  const IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  // Get from an empty portal.
  char data[4];
  size_t num_bytes = 4;
  EXPECT_EQ(IPCZ_RESULT_UNAVAILABLE, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data,
                                                &num_bytes, nullptr, nullptr));

  // A portal can't transfer itself or its peer.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Put(a, nullptr, 0, &a, 1, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Put(a, nullptr, 0, &b, 1, IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, "hi", 2, nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, "bye", 3, nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, nullptr, 0, nullptr, 0, IPCZ_NO_FLAGS, nullptr));

  auto [c, d] = OpenPortals(node);
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Put(a, nullptr, 0, &d, 1, IPCZ_NO_FLAGS, nullptr));
  d = IPCZ_INVALID_HANDLE;

  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(4u, status.num_local_parcels);
  EXPECT_EQ(5u, status.num_local_bytes);

  // Insufficient data storage.
  num_bytes = 0;
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data, &num_bytes, nullptr,
                       nullptr));
  EXPECT_EQ(2u, num_bytes);

  num_bytes = 4;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data,
                                       &num_bytes, nullptr, nullptr));
  EXPECT_EQ(2u, num_bytes);
  EXPECT_EQ("hi", std::string(data, 2));

  num_bytes = 4;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, data,
                                       &num_bytes, nullptr, nullptr));
  EXPECT_EQ(3u, num_bytes);
  EXPECT_EQ("bye", std::string(data, 3));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(2u, status.num_local_parcels);
  EXPECT_EQ(0u, status.num_local_bytes);

  // Getting an empty parcel requires no storage.
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, nullptr,
                                       nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(1u, status.num_local_parcels);
  EXPECT_EQ(0u, status.num_local_bytes);

  // Insufficient handle storage.
  EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
            ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr, nullptr,
                       nullptr));

  size_t num_handles = 1;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(b, IPCZ_NO_FLAGS, nullptr, nullptr,
                                       nullptr, &d, &num_handles));
  EXPECT_EQ(1u, num_handles);
  Close(d);

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(c, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_EQ(IPCZ_PORTAL_STATUS_PEER_CLOSED,
            status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);
  EXPECT_EQ(IPCZ_PORTAL_STATUS_DEAD, status.flags & IPCZ_PORTAL_STATUS_DEAD);

  CloseAll({a, b, c, node});
}

TEST_F(APITest, BeginEndPutFailure) {
  const IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  // Invalid portal.
  constexpr size_t kPutSize = 64;
  size_t num_bytes = kPutSize;
  void* data;
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().BeginPut(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr,
                            &num_bytes, &data));

  // Non-zero size but null data.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().BeginPut(a, IPCZ_NO_FLAGS, nullptr, &num_bytes, nullptr));

  // Invalid options.
  IpczBeginPutOptions options = {.size = 0};
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().BeginPut(a, IPCZ_NO_FLAGS, &options, &num_bytes, &data));

  // Duplicate two-phase Put.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginPut(a, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_ALREADY_EXISTS,
            ipcz().BeginPut(a, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr));

  // Invalid portal.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().EndPut(IPCZ_INVALID_HANDLE, 0, nullptr, 0, IPCZ_NO_FLAGS,
                          nullptr));

  // Non-zero number of handles, but null handle buffer.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().EndPut(a, 0, nullptr, 1, IPCZ_NO_FLAGS, nullptr));

  // Oversized data.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().EndPut(a, kPutSize * 2, nullptr, 0, IPCZ_NO_FLAGS, nullptr));

  // Invalid handle attachment.
  IpczHandle invalid_handle = IPCZ_INVALID_HANDLE;
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().EndPut(a, 0, &invalid_handle, 1, IPCZ_NO_FLAGS, nullptr));

  // Two-phase Put not in progress.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndPut(a, 0, nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            ipcz().EndPut(a, 0, nullptr, 0, IPCZ_NO_FLAGS, nullptr));

  CloseAll({a, b, node});
}

TEST_F(APITest, BeginEndGetFailure) {
  const IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  // No parcel yet.
  EXPECT_EQ(
      IPCZ_RESULT_UNAVAILABLE,
      ipcz().BeginGet(a, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr, nullptr));

  constexpr std::string_view kMessage = "ipcz";
  EXPECT_EQ(IPCZ_RESULT_OK, Put(b, kMessage));

  // Invalid portal.
  const void* data;
  size_t num_bytes;
  size_t num_handles;
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().BeginGet(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, &data,
                            &num_bytes, &num_handles));

  // No storage for data.
  EXPECT_EQ(
      IPCZ_RESULT_RESOURCE_EXHAUSTED,
      ipcz().BeginGet(a, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr, nullptr));
  EXPECT_EQ(
      IPCZ_RESULT_RESOURCE_EXHAUSTED,
      ipcz().BeginGet(a, IPCZ_NO_FLAGS, nullptr, &data, nullptr, nullptr));
  EXPECT_EQ(
      IPCZ_RESULT_RESOURCE_EXHAUSTED,
      ipcz().BeginGet(a, IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(a, IPCZ_NO_FLAGS, nullptr, &data,
                                            &num_bytes, nullptr));

  // Invalid handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().EndGet(IPCZ_INVALID_HANDLE, 0, 0, IPCZ_NO_FLAGS, nullptr,
                          nullptr));

  // Non-zero handle count with null handle buffer.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().EndGet(a, 0, 1, IPCZ_NO_FLAGS, nullptr, nullptr));

  // Data size out of range.
  EXPECT_EQ(
      IPCZ_RESULT_OUT_OF_RANGE,
      ipcz().EndGet(a, num_bytes + 1, 0, IPCZ_NO_FLAGS, nullptr, nullptr));

  // Two-phase Get not in progress.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(a, num_bytes, 0, IPCZ_NO_FLAGS, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_FAILED_PRECONDITION,
            ipcz().EndGet(a, num_bytes, 0, IPCZ_NO_FLAGS, nullptr, nullptr));

  CloseAll({a, b, node});
}

TEST_F(APITest, TwoPhasePutGet) {
  const IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  constexpr std::string_view kMessage = "ipcz!";
  size_t num_bytes = kMessage.size();
  void* out_data;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().BeginPut(a, IPCZ_NO_FLAGS, nullptr, &num_bytes, &out_data));
  EXPECT_EQ(kMessage.size(), num_bytes);
  memcpy(out_data, kMessage.data(), kMessage.size());
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndPut(a, num_bytes, nullptr, 0, IPCZ_NO_FLAGS, nullptr));

  const void* in_data;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(b, IPCZ_NO_FLAGS, nullptr, &in_data,
                                            &num_bytes, nullptr));
  EXPECT_EQ(kMessage[0], *reinterpret_cast<const char*>(in_data));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(b, 1, 0, IPCZ_NO_FLAGS, nullptr, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().BeginGet(b, IPCZ_NO_FLAGS, nullptr, &in_data,
                                            &num_bytes, nullptr));
  EXPECT_EQ(
      kMessage.substr(1),
      std::string_view(reinterpret_cast<const char*>(in_data), num_bytes));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().EndGet(b, num_bytes, 0, IPCZ_NO_FLAGS, nullptr, nullptr));

  EXPECT_EQ(
      IPCZ_RESULT_UNAVAILABLE,
      ipcz().BeginGet(b, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr, nullptr));

  CloseAll({a, b, node});
}

TEST_F(APITest, TrapInvalid) {
  const IpczHandle node = CreateNode(kDefaultDriver);
  auto [a, b] = OpenPortals(node);

  const auto handler = [](const IpczTrapEvent* event) {};

  // Null conditions.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Trap(b, nullptr, handler, 0, IPCZ_NO_FLAGS, nullptr, nullptr,
                        nullptr));

  // Invalid conditions.
  IpczTrapConditions conditions = {.size = sizeof(conditions) - 1};
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Trap(b, &conditions, handler, 0, IPCZ_NO_FLAGS, nullptr,
                        nullptr, nullptr));

  // Null handler.
  conditions = {.size = sizeof(conditions)};
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Trap(b, &conditions, nullptr, 0, IPCZ_NO_FLAGS, nullptr,
                        nullptr, nullptr));

  // Invalid or non-portal handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Trap(IPCZ_INVALID_HANDLE, &conditions, handler, 0,
                        IPCZ_NO_FLAGS, nullptr, nullptr, nullptr));
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Trap(node, &conditions, handler, 0, IPCZ_NO_FLAGS, nullptr,
                        nullptr, nullptr));

  // Invalid non-null output status.
  IpczPortalStatus status = {.size = sizeof(status) - 1};
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Trap(b, &conditions, handler, 0, IPCZ_NO_FLAGS, nullptr,
                        nullptr, &status));

  CloseAll({a, b, node});
}

TEST_F(APITest, BoxInvalid) {
  IpczDriverHandle transport0, transport1;
  ASSERT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.CreateTransports(
                IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
                IPCZ_NO_FLAGS, nullptr, &transport0, &transport1));
  EXPECT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.Close(transport1, IPCZ_NO_FLAGS, nullptr));

  IpczHandle node = CreateNode(kDefaultDriver);

  IpczHandle box;

  // Invalid node handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Box(IPCZ_INVALID_HANDLE, transport0, IPCZ_NO_FLAGS, nullptr,
                       &box));

  // Invalid driver handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Box(node, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS, nullptr,
                       &box));

  // Null output handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Box(node, transport0, IPCZ_NO_FLAGS, nullptr, nullptr));

  EXPECT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.Close(transport0, IPCZ_NO_FLAGS, nullptr));

  Close(node);
}

TEST_F(APITest, UnboxInvalid) {
  IpczDriverHandle transport0, transport1;
  ASSERT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.CreateTransports(
                IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
                IPCZ_NO_FLAGS, nullptr, &transport0, &transport1));
  EXPECT_EQ(IPCZ_RESULT_OK,
            kDefaultDriver.Close(transport1, IPCZ_NO_FLAGS, nullptr));

  IpczHandle node = CreateNode(kDefaultDriver);
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node, transport0, IPCZ_NO_FLAGS, nullptr, &box));

  IpczDriverHandle handle;

  // Null box handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Unbox(IPCZ_INVALID_HANDLE, IPCZ_NO_FLAGS, nullptr, &handle));

  // Invalid box handle type (node instead of box).
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Unbox(node, IPCZ_NO_FLAGS, nullptr, &handle));

  // Null output handle.
  EXPECT_EQ(IPCZ_RESULT_INVALID_ARGUMENT,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, nullptr));

  CloseAll({box, node});
}

}  // namespace
}  // namespace ipcz
