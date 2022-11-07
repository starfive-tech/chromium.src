// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/context_provider_for_test_v2.h"

#include <lib/sys/cpp/service_directory.h>

#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia_web/common/test/fake_feedback_service.h"
#include "fuchsia_web/common/test/test_realm_support.h"

namespace {

std::pair<std::unique_ptr<test::FakeFeedbackService>,
          ::component_testing::RealmRoot>
BuildRealm(base::CommandLine command_line) {
  DCHECK(command_line.argv()[0].empty()) << "Must use NO_PROGRAM.";

  auto realm_builder = ::component_testing::RealmBuilder::Create();

  static constexpr char kContextProviderService[] = "context_provider";
  realm_builder.AddChild(kContextProviderService, "#meta/context_provider.cm");

  // Append the arguments in `command_line` to those given to the binary.
  auto context_provider_decl =
      realm_builder.GetComponentDecl(kContextProviderService);
  for (auto& entry : *context_provider_decl.mutable_program()
                          ->mutable_info()
                          ->mutable_entries()) {
    if (entry.key == "args") {
      DCHECK(entry.value->is_str_vec());
      entry.value->str_vec().insert(entry.value->str_vec().end(),
                                    command_line.argv().begin() + 1,
                                    command_line.argv().end());
      break;
    }
  }
  realm_builder.ReplaceComponentDecl(kContextProviderService,
                                     std::move(context_provider_decl));

  auto fake_feedback_service = std::make_unique<test::FakeFeedbackService>(
      realm_builder, kContextProviderService);

  test::AddSyslogRoutesFromParent(realm_builder, kContextProviderService);

  realm_builder
      .AddRoute(::component_testing::Route{
          .capabilities = {::component_testing::Protocol{
                               "fuchsia.sys.Environment"},
                           ::component_testing::Protocol{"fuchsia.sys.Loader"}},
          .source = ::component_testing::ParentRef{},
          .targets = {::component_testing::ChildRef{kContextProviderService}}})
      .AddRoute(::component_testing::Route{
          .capabilities = {::component_testing::Protocol{
                               "fuchsia.web.ContextProvider"},
                           ::component_testing::Protocol{"fuchsia.web.Debug"}},
          .source = ::component_testing::ChildRef{kContextProviderService},
          .targets = {::component_testing::ParentRef{}}});

  return {std::move(fake_feedback_service), realm_builder.Build()};
}

}  // namespace

// static
ContextProviderForTest ContextProviderForTest::Create(
    const base::CommandLine& command_line) {
  auto [fake_feedback_service, realm_root] = BuildRealm(command_line);
  ::fuchsia::web::ContextProviderPtr context_provider;
  zx_status_t status = realm_root.Connect(context_provider.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect to ContextProvider";
  return ContextProviderForTest(std::move(fake_feedback_service),
                                std::move(realm_root),
                                std::move(context_provider));
}

ContextProviderForTest::ContextProviderForTest(
    ContextProviderForTest&&) noexcept = default;
ContextProviderForTest& ContextProviderForTest::operator=(
    ContextProviderForTest&&) noexcept = default;
ContextProviderForTest::~ContextProviderForTest() = default;

ContextProviderForTest::ContextProviderForTest(
    std::unique_ptr<test::FakeFeedbackService> fake_feedback_service,
    ::component_testing::RealmRoot realm_root,
    ::fuchsia::web::ContextProviderPtr context_provider)
    : fake_feedback_service_(std::move(fake_feedback_service)),
      realm_root_(std::move(realm_root)),
      context_provider_(std::move(context_provider)) {}

// static
ContextProviderForDebugTest ContextProviderForDebugTest::Create(
    const base::CommandLine& command_line) {
  return ContextProviderForDebugTest(
      ContextProviderForTest::Create(command_line));
}

ContextProviderForDebugTest::ContextProviderForDebugTest(
    ContextProviderForDebugTest&&) noexcept = default;
ContextProviderForDebugTest& ContextProviderForDebugTest::operator=(
    ContextProviderForDebugTest&&) noexcept = default;
ContextProviderForDebugTest::~ContextProviderForDebugTest() = default;

void ContextProviderForDebugTest::ConnectToDebug(
    ::fidl::InterfaceRequest<::fuchsia::web::Debug> debug_request) {
  zx_status_t status =
      context_provider_.realm_root().Connect(std::move(debug_request));
  ZX_CHECK(status == ZX_OK, status) << "Connect to Debug";
}

ContextProviderForDebugTest::ContextProviderForDebugTest(
    ContextProviderForTest context_provider)
    : context_provider_(std::move(context_provider)) {}
