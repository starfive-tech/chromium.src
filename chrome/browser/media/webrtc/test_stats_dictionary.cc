// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/test_stats_dictionary.h"

#include <memory>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

TestStatsReportDictionary::TestStatsReportDictionary(
    std::unique_ptr<base::DictionaryValue> report)
    : report_(std::move(report)) {
  CHECK(report_);
}

TestStatsReportDictionary::~TestStatsReportDictionary() {
}

void TestStatsReportDictionary::ForEach(
    std::function<void(const TestStatsDictionary&)> iteration) {
  for (auto it : report_->DictItems()) {
    const base::DictionaryValue* it_value;
    CHECK(it.second.GetAsDictionary(&it_value));
    iteration(TestStatsDictionary(this, it_value));
  }
}

std::vector<TestStatsDictionary> TestStatsReportDictionary::Filter(
    std::function<bool(const TestStatsDictionary&)> filter) {
  std::vector<TestStatsDictionary> result;
  ForEach([&result, &filter](const TestStatsDictionary& stats) {
    if (filter(stats))
      result.push_back(stats);
  });
  return result;
}

std::unique_ptr<TestStatsDictionary> TestStatsReportDictionary::Get(
    const std::string& id) {
  const base::DictionaryValue* dictionary;
  if (!report_->GetDictionary(id, &dictionary))
    return nullptr;
  return std::make_unique<TestStatsDictionary>(this, dictionary);
}

std::vector<TestStatsDictionary> TestStatsReportDictionary::GetAll() {
  return Filter([](const TestStatsDictionary&) { return true; });
}

std::vector<TestStatsDictionary> TestStatsReportDictionary::GetByType(
    const std::string& type) {
  return Filter([&type](const TestStatsDictionary& stats) {
    return stats.GetString("type") == type;
  });
}

TestStatsDictionary::TestStatsDictionary(
    TestStatsReportDictionary* report, const base::DictionaryValue* stats)
    : report_(report), stats_(stats) {
  CHECK(report_);
  CHECK(stats_);
}

TestStatsDictionary::TestStatsDictionary(
    const TestStatsDictionary& other) = default;

TestStatsDictionary::~TestStatsDictionary() {
}

bool TestStatsDictionary::IsBoolean(const std::string& key) const {
  bool value;
  return GetBoolean(key, &value);
}

bool TestStatsDictionary::GetBoolean(const std::string& key) const {
  bool value;
  CHECK(GetBoolean(key, &value));
  return value;
}

bool TestStatsDictionary::IsNumber(const std::string& key) const {
  double value;
  return GetNumber(key, &value);
}

double TestStatsDictionary::GetNumber(const std::string& key) const {
  double value;
  CHECK(GetNumber(key, &value));
  return value;
}

bool TestStatsDictionary::IsString(const std::string& key) const {
  return stats_->GetDict().FindString(key) != nullptr;
}

std::string TestStatsDictionary::GetString(const std::string& key) const {
  const std::string* value = stats_->GetDict().FindString(key);
  CHECK(value);
  return *value;
}

bool TestStatsDictionary::IsSequenceBoolean(const std::string& key) const {
  std::vector<bool> value;
  return GetSequenceBoolean(key, &value);
}

std::vector<bool> TestStatsDictionary::GetSequenceBoolean(
    const std::string& key) const {
  std::vector<bool> value;
  CHECK(GetSequenceBoolean(key, &value));
  return value;
}

bool TestStatsDictionary::IsSequenceNumber(const std::string& key) const {
  std::vector<double> value;
  return GetSequenceNumber(key, &value);
}

std::vector<double> TestStatsDictionary::GetSequenceNumber(
    const std::string& key) const {
  std::vector<double> value;
  CHECK(GetSequenceNumber(key, &value));
  return value;
}

bool TestStatsDictionary::IsSequenceString(const std::string& key) const {
  std::vector<std::string> value;
  return GetSequenceString(key, &value);
}

std::vector<std::string> TestStatsDictionary::GetSequenceString(
    const std::string& key) const {
  std::vector<std::string> value;
  CHECK(GetSequenceString(key, &value));
  return value;
}

bool TestStatsDictionary::GetBoolean(
    const std::string& key, bool* out) const {
  if (absl::optional<bool> value = stats_->FindBoolPath(key)) {
    *out = *value;
    return true;
  }
  return false;
}

bool TestStatsDictionary::GetNumber(
    const std::string& key, double* out) const {
  if (absl::optional<double> value = stats_->FindDoubleKey(key)) {
    *out = *value;
    return true;
  }
  return false;
}

bool TestStatsDictionary::GetSequenceBoolean(
    const std::string& key,
    std::vector<bool>* out) const {
  const base::ListValue* list;
  if (!stats_->GetList(key, &list))
    return false;
  std::vector<bool> sequence;
  base::Value::ConstListView args_list = list->GetListDeprecated();
  for (const base::Value& arg : args_list) {
    if (!arg.is_bool())
      return false;
    sequence.push_back(arg.GetBool());
  }
  *out = std::move(sequence);
  return true;
}

bool TestStatsDictionary::GetSequenceNumber(
    const std::string& key,
    std::vector<double>* out) const {
  const base::Value* number_sequence = stats_->FindListKey(key);
  if (!number_sequence)
    return false;

  out->clear();
  for (const base::Value& element : number_sequence->GetListDeprecated()) {
    absl::optional<double> double_value = element.GetIfDouble();
    if (!double_value)
      return false;

    out->push_back(*double_value);
  }

  return true;
}

bool TestStatsDictionary::GetSequenceString(
    const std::string& key,
    std::vector<std::string>* out) const {
  const base::ListValue* list;
  if (!stats_->GetList(key, &list))
    return false;
  std::vector<std::string> sequence;
  for (const base::Value& i : list->GetListDeprecated()) {
    const std::string* element = i.GetIfString();
    if (!element)
      return false;
    sequence.push_back(*element);
  }
  *out = std::move(sequence);
  return true;
}

std::string TestStatsDictionary::ToString() const {
  std::string str;
  CHECK(base::JSONWriter::WriteWithOptions(
      *stats_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &str));
  return str;
}

}  // namespace content
