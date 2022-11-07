// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_client.h"

namespace enterprise_connectors {

FakeContentAnalysisSdkClient::FakeContentAnalysisSdkClient(
    const content_analysis::sdk::Client::Config& config)
    : config_(config) {}

FakeContentAnalysisSdkClient::~FakeContentAnalysisSdkClient() = default;

const content_analysis::sdk::Client::Config&
FakeContentAnalysisSdkClient::GetConfig() const {
  return config_;
}

int FakeContentAnalysisSdkClient::Send(
    const content_analysis::sdk::ContentAnalysisRequest& request,
    content_analysis::sdk::ContentAnalysisResponse* response) {
  request_ = request;
  // To correlate request and response, just like what the real agent should do.
  response_.set_request_token(request_.request_token());
  *response = response_;
  return send_status_;
}

int FakeContentAnalysisSdkClient::Acknowledge(
    const content_analysis::sdk::ContentAnalysisAcknowledgement& ack) {
  return ack_status_;
}

const content_analysis::sdk::ContentAnalysisRequest&
FakeContentAnalysisSdkClient::GetRequest() {
  return request_;
}

void FakeContentAnalysisSdkClient::SetAckStatus(int status) {
  ack_status_ = status;
}

void FakeContentAnalysisSdkClient::SetSendStatus(int status) {
  send_status_ = status;
}

void FakeContentAnalysisSdkClient::SetSendResponse(
    const content_analysis::sdk::ContentAnalysisResponse& response) {
  response_ = response;
}

}  // namespace enterprise_connectors
