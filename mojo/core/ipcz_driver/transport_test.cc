// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/transport.h"

#include <algorithm>
#include <cstring>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "mojo/core/ipcz_driver/transmissible_platform_handle.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo::core::ipcz_driver {
namespace {

struct TestMessage {
  TestMessage() = default;
  explicit TestMessage(std::string_view str,
                       base::span<IpczDriverHandle> handles = {})
      : bytes(str.begin(), str.end()),
        handles(handles.begin(), handles.end()) {}

  std::string as_string() const {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
  }

  void Transmit(Transport& transmitter) {
    transmitter.Transmit(base::make_span(bytes), base::make_span(handles));
  }

  std::vector<uint8_t> bytes;
  std::vector<IpczDriverHandle> handles;
};

// These tests use Mojo and Mojo's existing multiprocess test facilities to set
// up a multiprocess environment and send an initial transport handle to the
// child process.
class MojoIpczTransportTest : public test::MojoTestBase {
 protected:
  // Creates a new ad hoc ipcz Transport object from a new PlatformChannel. One
  // end of the channel is returned as a Transport while the other is sent over
  // `pipe` to `process`.
  static scoped_refptr<Transport> CreateAndSendTransport(
      MojoHandle pipe,
      const base::Process& process) {
    PlatformChannel channel;
    MojoHandle transport_for_client =
        WrapPlatformHandle(channel.TakeRemoteEndpoint().TakePlatformHandle())
            .release()
            .value();
    WriteMessageWithHandles(pipe, "", &transport_for_client, 1);
    return base::MakeRefCounted<Transport>(Transport::kToNonBroker,
                                           channel.TakeLocalEndpoint(),
                                           process.Duplicate());
  }

  // Retrieves a PlatformChannel endpoint from `pipe` and returns a newly
  // constructed Transport over it.
  static scoped_refptr<Transport> ReceiveTransport(MojoHandle pipe) {
    MojoHandle transport_for_client;
    ReadMessageWithHandles(pipe, &transport_for_client, 1);
    PlatformHandle handle =
        UnwrapPlatformHandle(ScopedHandle(Handle(transport_for_client)));
    return base::MakeRefCounted<Transport>(
        Transport::kToBroker, PlatformChannelEndpoint(std::move(handle)));
  }

  static TestMessage SerializeObjectFor(Transport& transmitter,
                                        scoped_refptr<ObjectBase> object) {
    size_t num_bytes = 0;
    size_t num_handles = 0;
    EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
              transmitter.SerializeObject(*object, nullptr, &num_bytes, nullptr,
                                          &num_handles));

    TestMessage message;
    message.bytes.resize(num_bytes);
    message.handles.resize(num_handles);
    EXPECT_EQ(IPCZ_RESULT_OK, transmitter.SerializeObject(
                                  *object, message.bytes.data(), &num_bytes,
                                  message.handles.data(), &num_handles));
    return message;
  }

  template <typename T>
  static scoped_refptr<T> DeserializeObjectFrom(Transport& receiver,
                                                const TestMessage& message) {
    scoped_refptr<ObjectBase> object;
    const IpczResult result =
        receiver.DeserializeObject(base::make_span(message.bytes),
                                   base::make_span(message.handles), object);
    CHECK_EQ(result, IPCZ_RESULT_OK);
    CHECK_EQ(object->type(), T::object_type());
    return base::WrapRefCounted(static_cast<T*>(object.get()));
  }

  static TestMessage SerializeFileFor(Transport& transmitter, base::File file) {
    auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
        PlatformHandle(base::ScopedPlatformFile(file.TakePlatformFile())));
    return SerializeObjectFor(transmitter, std::move(wrapper));
  }

  static base::File DeserializeFileFrom(Transport& receiver,
                                        const TestMessage& message) {
    scoped_refptr<WrappedPlatformHandle> wrapper =
        DeserializeObjectFrom<WrappedPlatformHandle>(receiver, message);
    CHECK(wrapper);
#if BUILDFLAG(IS_WIN)
    return base::File(wrapper->TakeHandle().TakeHandle());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    return base::File(wrapper->TakeHandle().TakeFD());
#endif
  }
};

// TransportListener provides a convenient way for tests to listen to incoming
// events on a Transport.
class TransportListener {
 public:
  explicit TransportListener(Transport& transport) : transport_(transport) {
    transport_.Activate(reinterpret_cast<IpczHandle>(this),
                        &TransportListener::OnActivity);
  }

  ~TransportListener() {
    transport_.Deactivate();
    deactivation_event_.Wait();
  }

  TestMessage WaitForNextMessage() {
    base::AutoLock lock(lock_);
    while (messages_.empty()) {
      have_messages_.Wait();
    }

    TestMessage message = std::move(messages_.front());
    messages_.pop();
    return message;
  }

  void WaitForDisconnect() { disconnect_event_.Wait(); }

 private:
  static IpczResult OnActivity(IpczHandle transport,
                               const void* data,
                               size_t num_bytes,
                               const IpczDriverHandle* handles,
                               size_t num_handles,
                               IpczTransportActivityFlags flags,
                               const void*) {
    auto* listener = reinterpret_cast<TransportListener*>(transport);
    auto bytes = base::make_span(static_cast<const uint8_t*>(data), num_bytes);
    listener->HandleActivity(bytes, base::make_span(handles, num_handles),
                             flags);
    return IPCZ_RESULT_OK;
  }

  void HandleActivity(base::span<const uint8_t> bytes,
                      base::span<const IpczDriverHandle> handles,
                      IpczTransportActivityFlags flags) {
    if (flags & IPCZ_TRANSPORT_ACTIVITY_ERROR) {
      disconnect_event_.Signal();
      return;
    }

    if (flags & IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED) {
      deactivation_event_.Signal();
      return;
    }

    TestMessage message;
    message.bytes.resize(bytes.size());
    message.handles.resize(handles.size());
    std::copy(bytes.begin(), bytes.end(), message.bytes.begin());
    std::copy(handles.begin(), handles.end(), message.handles.begin());

    base::AutoLock lock(lock_);
    messages_.push(std::move(message));
    have_messages_.Signal();
  }

  Transport& transport_;

  base::Lock lock_;
  base::ConditionVariable have_messages_{&lock_};
  std::queue<TestMessage> messages_ GUARDED_BY(lock_);
  base::WaitableEvent disconnect_event_;
  base::WaitableEvent deactivation_event_;
};

constexpr std::string_view kMessage1 = "we are messages";
constexpr std::string_view kMessage2 = "tremendous messages";
constexpr std::string_view kMessage3 = "the very best messages";
constexpr std::string_view kMessage4 = "everyone says so";

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(BasicTransmitClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);
  TestMessage(kMessage3).Transmit(*transport);
  TestMessage(kMessage4).Transmit(*transport);
  EXPECT_EQ(kMessage1, listener.WaitForNextMessage().as_string());
  EXPECT_EQ(kMessage2, listener.WaitForNextMessage().as_string());
}

TEST_F(MojoIpczTransportTest, BasicTransmit) {
  RunTestClientWithController("BasicTransmitClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process());

    TransportListener listener(*transport);
    TestMessage(kMessage1).Transmit(*transport);
    TestMessage(kMessage2).Transmit(*transport);
    EXPECT_EQ(kMessage3, listener.WaitForNextMessage().as_string());
    EXPECT_EQ(kMessage4, listener.WaitForNextMessage().as_string());
    listener.WaitForDisconnect();
  });
}

// Transport on Windows does not support out-of-band handle transfer, so this
// test is impossible there. Windows handle transmission is instead covered by
// tests which more broadly cover driver object serialization.
#if !BUILDFLAG(IS_WIN)
IpczDriverHandle MakeHandleFromEndpoint(PlatformChannelEndpoint endpoint) {
  return TransmissiblePlatformHandle::ReleaseAsHandle(
      base::MakeRefCounted<TransmissiblePlatformHandle>(
          endpoint.TakePlatformHandle()));
}

scoped_refptr<Transport> MakeTransportFromMessage(const TestMessage& message) {
  CHECK_EQ(message.handles.size(), 1u);
  auto handle = TransmissiblePlatformHandle::TakeFromHandle(message.handles[0]);
  CHECK(handle);
  return base::MakeRefCounted<Transport>(
      Transport::kToBroker, PlatformChannelEndpoint(handle->TakeHandle()));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitHandleClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  scoped_refptr<Transport> new_transport1;
  scoped_refptr<Transport> new_transport2;
  {
    TransportListener listener(*transport);
    new_transport1 = MakeTransportFromMessage(listener.WaitForNextMessage());
    new_transport2 = MakeTransportFromMessage(listener.WaitForNextMessage());
  }

  TransportListener listener1(*new_transport1);
  TransportListener listener2(*new_transport2);
  TestMessage(kMessage3).Transmit(*new_transport1);
  TestMessage(kMessage4).Transmit(*new_transport2);
  EXPECT_EQ(kMessage1, listener1.WaitForNextMessage().as_string());
  EXPECT_EQ(kMessage2, listener2.WaitForNextMessage().as_string());
}

TEST_F(MojoIpczTransportTest, TransmitHandle) {
  RunTestClientWithController("TransmitHandleClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process());

    // The PlatformHandle backing a PlatformChannelEndpoint is already
    // transmissible on all applicable platforms, so we can conveniently test
    // handle transmission without depending on driver object serialization.
    PlatformChannel channel1;
    auto new_transport1 = base::MakeRefCounted<Transport>(
        Transport::kToNonBroker, channel1.TakeLocalEndpoint(),
        c.process().Duplicate());

    PlatformChannel channel2;
    auto new_transport2 = base::MakeRefCounted<Transport>(
        Transport::kToNonBroker, channel2.TakeLocalEndpoint(),
        c.process().Duplicate());

    IpczDriverHandle handle1 =
        MakeHandleFromEndpoint(channel1.TakeRemoteEndpoint());
    IpczDriverHandle handle2 =
        MakeHandleFromEndpoint(channel2.TakeRemoteEndpoint());
    {
      TransportListener listener(*transport);
      TestMessage("!", {&handle1, 1}).Transmit(*transport);
      TestMessage("!", {&handle2, 1}).Transmit(*transport);
      listener.WaitForDisconnect();
    }

    TransportListener listener1(*new_transport1);
    TransportListener listener2(*new_transport2);
    TestMessage(kMessage1).Transmit(*new_transport1);
    TestMessage(kMessage2).Transmit(*new_transport2);
    EXPECT_EQ(kMessage3, listener1.WaitForNextMessage().as_string());
    EXPECT_EQ(kMessage4, listener2.WaitForNextMessage().as_string());
    listener1.WaitForDisconnect();
    listener2.WaitForDisconnect();
  });
}
#endif  // !BUILDFLAG(IS_WIN)

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitSerializedTransportClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  scoped_refptr<Transport> new_transport;
  {
    TransportListener listener(*transport);
    new_transport = DeserializeObjectFrom<Transport>(
        *transport, listener.WaitForNextMessage());
  }
  TransportListener listener(*new_transport);
  TestMessage(kMessage3).Transmit(*new_transport);
  TestMessage(kMessage4).Transmit(*new_transport);
  EXPECT_EQ(kMessage1, listener.WaitForNextMessage().as_string());
  EXPECT_EQ(kMessage2, listener.WaitForNextMessage().as_string());
}

TEST_F(MojoIpczTransportTest, TransmitSerializedTransport) {
  RunTestClientWithController(
      "TransmitSerializedTransportClient", [&](ClientController& c) {
        scoped_refptr<Transport> transport =
            CreateAndSendTransport(c.pipe(), c.process());

        auto [our_new_transport, their_new_transport] = Transport::CreatePair(
            Transport::kToNonBroker, Transport::kToBroker);
        {
          TransportListener listener(*transport);
          SerializeObjectFor(*transport, std::move(their_new_transport))
              .Transmit(*transport);
          listener.WaitForDisconnect();
        }

        TransportListener listener(*our_new_transport);
        TestMessage(kMessage1).Transmit(*our_new_transport);
        TestMessage(kMessage2).Transmit(*our_new_transport);
        EXPECT_EQ(kMessage3, listener.WaitForNextMessage().as_string());
        EXPECT_EQ(kMessage4, listener.WaitForNextMessage().as_string());
        listener.WaitForDisconnect();
      });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitFileClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);
  base::File file =
      DeserializeFileFrom(*transport, listener.WaitForNextMessage());

  std::vector<char> data(file.GetLength());
  file.Read(0, data.data(), data.size());
  EXPECT_EQ(kMessage1, std::string(data.begin(), data.end()));
}

TEST_F(MojoIpczTransportTest, TransmitFile) {
  RunTestClientWithController("TransmitFileClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process());

    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    base::File new_file(temp_dir.GetPath().AppendASCII("testfile"),
                        base::File::FLAG_CREATE | base::File::FLAG_READ |
                            base::File::FLAG_WRITE);
    new_file.Write(0, kMessage1.data(), kMessage1.size());

    TransportListener listener(*transport);
    SerializeFileFor(*transport, std::move(new_file)).Transmit(*transport);
    listener.WaitForDisconnect();
  });
}

}  // namespace
}  // namespace mojo::core::ipcz_driver
