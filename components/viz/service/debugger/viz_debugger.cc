// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <atomic>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/viz/service/debugger/viz_debugger.h"

#if VIZ_DEBUGGER_IS_ON()

#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_id_name_manager.h"

namespace viz {

// Version the protocol in case we ever want or need backwards compatibility
// support.
static const int kVizDebuggerVersion = 1;

std::atomic<bool> VizDebugger::enabled_;

VizDebugger::BufferInfo::BufferInfo() = default;
VizDebugger::BufferInfo::~BufferInfo() = default;
VizDebugger::BufferInfo::BufferInfo(const BufferInfo& a) = default;

VizDebugger* VizDebugger::GetInstance() {
  static VizDebugger g_debugger;
  return &g_debugger;
}

VizDebugger::FilterBlock::FilterBlock(const std::string file_str,
                                      const std::string func_str,
                                      const std::string anno_str,
                                      bool is_active,
                                      bool is_enabled)
    : file(std::move(file_str)),
      func(std::move(func_str)),
      anno(std::move(anno_str)),
      active(is_active),
      enabled(is_enabled) {}

VizDebugger::FilterBlock::~FilterBlock() = default;

VizDebugger::FilterBlock::FilterBlock(const FilterBlock& other) = default;

base::DictionaryValue VizDebugger::CallSubmitCommon::GetDictionaryValue()
    const {
  base::DictionaryValue option_dict;
  option_dict.SetString("color",
                        base::StringPrintf("#%02x%02x%02x", option.color_r,
                                           option.color_g, option.color_b));
  option_dict.SetInteger("alpha", option.color_a);

  base::DictionaryValue dict;
  dict.SetInteger("drawindex", draw_index);
  dict.SetInteger("source_index", source_index);
  dict.SetInteger("thread_id", thread_id);
  dict.SetKey("option", std::move(option_dict));
  return dict;
}

VizDebugger::StaticSource::StaticSource(const char* anno_name,
                                        const char* file_name,
                                        int file_line,
                                        const char* func_name)
    : anno(anno_name), file(file_name), func(func_name), line(file_line) {
  VizDebugger::GetInstance()->RegisterSource(this);
}

VizDebugger::VizDebugger()
    : gpu_thread_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  enabled_.store(false);
}

VizDebugger::~VizDebugger() = default;

void VizDebugger::SubmitBuffer(int buff_id, VizDebugger::BufferInfo buffer) {
  VizDebugger::Buffer buff;
  buff.id = buff_id;
  buff.buffer_info = buffer;
  buffers_.emplace_back(buff);
}

base::Value VizDebugger::FrameAsJson(const uint64_t counter,
                                     const gfx::Size& window_pix,
                                     base::TimeTicks time_ticks) {
  // TODO(petermcneeley): When we move to multithread we need to do something
  // like an atomic swap here. Currently all multithreading concerns are handled
  // by having a lock around the |json_frame_output_| object.
  submission_count_ = 0;

  base::DictionaryValue global_dict;
  global_dict.SetInteger("version", kVizDebuggerVersion);
  global_dict.SetString("frame", base::NumberToString(counter));
  global_dict.SetInteger("windowx", window_pix.width());
  global_dict.SetInteger("windowy", window_pix.height());
  global_dict.SetString(
      "time", base::NumberToString(time_ticks.since_origin().InMicroseconds()));

  base::ListValue new_sources;
  for (size_t i = last_sent_source_count_; i < sources_.size(); i++) {
    const StaticSource* each = sources_[i];

    base::DictionaryValue dict;
    dict.SetString("file", each->file);
    dict.SetInteger("line", each->line);
    dict.SetString("func", each->func);
    dict.SetString("anno", each->anno);
    dict.SetInteger("index", each->reg_index);
    new_sources.Append(std::move(dict));
  }

  // Remote connection will now have acknowledged all the new sources.
  last_sent_source_count_ = sources_.size();
  global_dict.SetKey("new_sources", std::move(new_sources));

  // We take the minimum between tail index and buffer size to make sure we
  // don't go out of bounds.
  size_t const max_rect_calls_index =
      std::min(static_cast<int>(draw_rect_calls_tail_idx_),
               static_cast<int>(draw_rect_calls_.size()));
  size_t const max_text_calls_index =
      std::min(static_cast<int>(draw_text_calls_tail_idx_),
               static_cast<int>(draw_text_calls_.size()));
  size_t const max_logs_index = std::min(static_cast<int>(logs_tail_idx_),
                                         static_cast<int>(logs_.size()));

  base::ListValue draw_calls;
  // We are also grabbing the active threads while parsing draw calls.
  base::ListValue new_threads;
  // Hash set to keep track of threads that have been registered already.
  base::flat_set<int> registered_threads;
  for (size_t i = 0; i < max_rect_calls_index; ++i) {
    base::DictionaryValue dict = draw_rect_calls_[i].GetDictionaryValue();
    base::DictionaryValue threads_dict;
    {
      base::ListValue list_xy;
      list_xy.Append(draw_rect_calls_[i].obj_size.width());
      list_xy.Append(draw_rect_calls_[i].obj_size.height());
      dict.SetKey("size", std::move(list_xy));
    }
    {
      base::ListValue list_xy;
      list_xy.Append(static_cast<double>(draw_rect_calls_[i].pos.x()));
      list_xy.Append(static_cast<double>(draw_rect_calls_[i].pos.y()));
      dict.SetKey("pos", std::move(list_xy));
    }
    if (draw_rect_calls_[i].uv != DEFAULT_UV) {
      {
        base::ListValue list_xy;
        list_xy.Append(static_cast<double>(draw_rect_calls_[i].uv.width()));
        list_xy.Append(static_cast<double>(draw_rect_calls_[i].uv.height()));
        dict.SetKey("uv_size", std::move(list_xy));
      }
      {
        base::ListValue list_xy;
        list_xy.Append(static_cast<double>(draw_rect_calls_[i].uv.x()));
        list_xy.Append(static_cast<double>(draw_rect_calls_[i].uv.y()));
        dict.SetKey("uv_pos", std::move(list_xy));
      }
    }
    dict.SetInteger("buff_id", std::move(draw_rect_calls_[i].buff_id));

    // Thread ID and Name processing stuff.
    int cur_thread_id = draw_rect_calls_[i].thread_id;
    // If new thread is not registered yet, then register it and mark it as
    // registered.
    if (registered_threads.find(cur_thread_id) == registered_threads.end()) {
      std::string cur_thread_name =
          base::ThreadIdNameManager::GetInstance()->GetName(cur_thread_id);
      threads_dict.SetInteger("thread_id", cur_thread_id);
      threads_dict.SetString("thread_name", cur_thread_name);
      new_threads.Append(std::move(threads_dict));
      registered_threads.insert(cur_thread_id);
    }

    draw_calls.Append(std::move(dict));
  }
  global_dict.SetKey("drawcalls", std::move(draw_calls));
  global_dict.SetKey("threads", std::move(new_threads));

  base::DictionaryValue buff_map;
  for (auto&& each : buffers_) {
    base::DictionaryValue dict;
    dict.SetInteger("width", each.buffer_info.width);
    dict.SetInteger("height", each.buffer_info.height);
    base::ListValue lst;
    for (auto& buffer : each.buffer_info.buffer) {
      lst.Append(buffer.color_r);
      lst.Append(buffer.color_g);
      lst.Append(buffer.color_b);
      lst.Append(buffer.color_a);
    }
    dict.SetKey("buffer", std::move(lst));
    buff_map.SetKey(base::NumberToString(each.id), std::move(dict));
  }
  global_dict.SetKey("buff_map", std::move(buff_map));

  base::ListValue logs;
  for (size_t i = 0; i < max_logs_index; ++i) {
    base::DictionaryValue dict = logs_[i].GetDictionaryValue();
    dict.SetString("value", std::move(logs_[i].value));
    logs.Append(std::move(dict));
  }
  global_dict.SetKey("logs", std::move(logs));

  base::ListValue texts;
  for (size_t i = 0; i < max_text_calls_index; ++i) {
    base::DictionaryValue dict = draw_text_calls_[i].GetDictionaryValue();
    {
      base::ListValue list_xy;
      list_xy.Append(static_cast<double>(draw_text_calls_[i].pos.x()));
      list_xy.Append(static_cast<double>(draw_text_calls_[i].pos.y()));
      dict.SetKey("pos", std::move(list_xy));
    }
    dict.SetString("text", draw_text_calls_[i].text);
    texts.Append(std::move(dict));
  }
  global_dict.SetKey("text", std::move(texts));

  // Reset index counters for each buffer.
  draw_rect_calls_tail_idx_ = 0;
  draw_text_calls_tail_idx_ = 0;
  logs_tail_idx_ = 0;

  return std::move(global_dict);
}

void VizDebugger::UpdateFilters() {
  if (apply_new_filters_next_frame_) {
    cached_filters_ = new_filters_;
    for (auto&& source : sources_) {
      ApplyFilters(source);
    }
    new_filters_.clear();
    apply_new_filters_next_frame_ = false;
  }
}

void VizDebugger::CompleteFrame(uint64_t counter,
                                const gfx::Size& window_pix,
                                base::TimeTicks time_ticks) {
  read_write_lock_.WriteLock();
  UpdateFilters();
  json_frame_output_ = FrameAsJson(counter, window_pix, time_ticks);
  gpu_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizDebugger::AddFrame, base::Unretained(this)));
  read_write_lock_.WriteUnLock();
}

void VizDebugger::ApplyFilters(VizDebugger::StaticSource* src) {
  // In the case of no filters we disable this source.
  src->active = false;
  src->enabled = false;
  // TODO(petermcneeley): We should probably make this string filtering more
  // optimal. However, for the most part it the cost is only paid on the
  // application of new filters.
  auto simple_match = [](const char* source_str,
                         const std::string& filter_match) {
    if (filter_match.empty() || source_str == nullptr) {
      return true;
    }
    return std::strstr(source_str, filter_match.c_str()) != nullptr;
  };

  for (const auto& filter_block : cached_filters_) {
    if (simple_match(src->file, filter_block.file) &&
        simple_match(src->func, filter_block.func) &&
        simple_match(src->anno, filter_block.anno)) {
      src->active = filter_block.active;
      src->enabled = filter_block.enabled;
    }
  }
}

void VizDebugger::RegisterSource(StaticSource* src) {
  read_write_lock_.WriteLock();
  int index = sources_.size();
  src->reg_index = index;
  ApplyFilters(src);
  sources_.push_back(src);
  read_write_lock_.WriteUnLock();
}

void VizDebugger::Draw(const gfx::SizeF& obj_size,
                       const gfx::Vector2dF& pos,
                       const VizDebugger::StaticSource* dcs,
                       VizDebugger::DrawOption option,
                       int* id,
                       const gfx::RectF& uv) {
  Draw(gfx::Size(obj_size.width(), obj_size.height()), pos, dcs, option, id,
       uv);
}

void VizDebugger::Draw(const gfx::Size& obj_size,
                       const gfx::Vector2dF& pos,
                       const VizDebugger::StaticSource* dcs,
                       VizDebugger::DrawOption option,
                       int* id,
                       const gfx::RectF& uv) {
  DrawInternal(obj_size, pos, dcs, option, id, uv);
}

void VizDebugger::DrawInternal(const gfx::Size& obj_size,
                               const gfx::Vector2dF& pos,
                               const VizDebugger::StaticSource* dcs,
                               VizDebugger::DrawOption option,
                               int* id,
                               const gfx::RectF& uv) {
  int local_id_buffer = -1;
  if (id != nullptr) {
    local_id_buffer = buffer_id++;
    *id = local_id_buffer;
  }

  //  Store atomic insertion index in local variable to use to insert into
  //  buffer.
  int insertion_index;

  for (;;) {
    read_write_lock_.ReadLock();
    // Get call insertion index.
    insertion_index = draw_rect_calls_tail_idx_++;
    // If the insertion index is within bounds, insert call into buffer.
    if (static_cast<size_t>(insertion_index) < draw_rect_calls_.size()) {
      int cur_thread_id = base::PlatformThread::CurrentId();
      draw_rect_calls_[insertion_index] = DrawCall{submission_count_++,
                                                   dcs->reg_index,
                                                   cur_thread_id,
                                                   option,
                                                   obj_size,
                                                   pos,
                                                   local_id_buffer,
                                                   uv};
      // Return when call insertion is successful.
      read_write_lock_.ReadUnlock();
      return;
    }
    read_write_lock_.ReadUnlock();
    // Take write lock to resize and re-adjust buffer tail index after buffer
    // overflow.
    read_write_lock_.WriteLock();
    // If tail index is over buffer size, then resizing is definitely needed.
    // Also re-adjust tail index so it's at the start of the new buffer space.
    if (static_cast<size_t>(draw_rect_calls_tail_idx_) >=
        draw_rect_calls_.size()) {
      draw_rect_calls_tail_idx_ = draw_rect_calls_.size();
      draw_rect_calls_.resize(draw_rect_calls_.size() * 2);
    }
    read_write_lock_.WriteUnLock();
  }
}

void VizDebugger::DrawText(const gfx::PointF& pos,
                           const std::string& text,
                           const VizDebugger::StaticSource* dcs,
                           VizDebugger::DrawOption option) {
  DrawText(gfx::Vector2dF(pos.OffsetFromOrigin()), text, dcs, option);
}

void VizDebugger::DrawText(const gfx::Point& pos,
                           const std::string& text,
                           const VizDebugger::StaticSource* dcs,
                           VizDebugger::DrawOption option) {
  DrawText(gfx::Vector2dF(pos.x(), pos.y()), text, dcs, option);
}

void VizDebugger::DrawText(const gfx::Vector2dF& pos,
                           const std::string& text,
                           const VizDebugger::StaticSource* dcs,
                           VizDebugger::DrawOption option) {
  //  Store atomic insertion index in local variable to use to insert into
  //  buffer.
  int insertion_index;

  for (;;) {
    read_write_lock_.ReadLock();
    // Get call insertion index.
    insertion_index = draw_text_calls_tail_idx_++;
    // If the insertion index is within bounds, insert call into buffer.
    if (static_cast<size_t>(insertion_index) < draw_text_calls_.size()) {
      int cur_thread_id = base::PlatformThread::CurrentId();
      draw_text_calls_[insertion_index] = DrawTextCall{submission_count_++,
                                                       dcs->reg_index,
                                                       cur_thread_id,
                                                       option,
                                                       pos,
                                                       text};
      // Return when call insertion is successful.
      read_write_lock_.ReadUnlock();
      return;
    }
    read_write_lock_.ReadUnlock();
    // Take write lock to resize and re-adjust buffer tail index after buffer
    // overflow.
    read_write_lock_.WriteLock();
    // If tail index is over buffer size, then resizing is definitely needed.
    // Also re-adjust tail index so it's at the start of the new buffer space.
    if (static_cast<size_t>(draw_text_calls_tail_idx_) >=
        draw_text_calls_.size()) {
      draw_text_calls_tail_idx_ = draw_text_calls_.size();
      draw_text_calls_.resize(draw_text_calls_.size() * 2);
    }
    read_write_lock_.WriteUnLock();
  }
}

void VizDebugger::AddFrame() {
  // TODO(petermcneeley): This code has duel thread entry. One to launch the
  // task and one for the task to run. We should improve on this design in the
  // future and have a better multithreaded frame data aggregation system.
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  if (debug_output_.is_bound()) {
    debug_output_->LogFrame(std::move(json_frame_output_));
  }
  read_write_lock_.WriteUnLock();
}

void VizDebugger::FilterDebugStream(base::Value json) {
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  const base::Value* value = &(json);
  const base::Value* filterlist = value->FindPath("filters");

  if (!filterlist || !filterlist->is_list()) {
    LOG(ERROR) << "Missing filter list in json: " << json;
    return;
  }

  new_filters_.clear();

  for (const auto& filter : filterlist->GetListDeprecated()) {
    const base::Value* file = filter.FindPath("selector.file");
    const base::Value* func = filter.FindPath("selector.func");
    const base::Value* anno = filter.FindPath("selector.anno");
    const base::Value* active = filter.FindPath("active");
    const base::Value* enabled = filter.FindPath("enabled");

    if (!active) {
      LOG(ERROR) << "Missing filter props in json: " << json;
      return;
    }

    if ((file && !file->is_string()) || (func && !func->is_string()) ||
        (anno && !anno->is_string()) || !active->is_bool()) {
      LOG(ERROR) << "Filter props wrong type in json: " << json;
      continue;
    }

    auto check_str = [](const base::Value* filter_str) {
      return (filter_str ? filter_str->GetString() : std::string());
    };

    new_filters_.emplace_back(
        check_str(file), check_str(func), check_str(anno), active->GetBool(),
        (enabled && enabled->is_bool()) ? enabled->GetBool() : true);
  }

  apply_new_filters_next_frame_ = true;
  read_write_lock_.WriteUnLock();
}

void VizDebugger::StartDebugStream(
    mojo::PendingRemote<mojom::VizDebugOutput> pending_debug_output) {
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  debug_output_.Bind(std::move(pending_debug_output));
  debug_output_.reset_on_disconnect();
  last_sent_source_count_ = 0;

  // Reset our filters for our new connection. By default the client will send
  // along the new filters after establishing the connection.
  new_filters_.clear();
  apply_new_filters_next_frame_ = true;

  base::DictionaryValue dict;
  dict.SetString("connection", "ok");
  debug_output_->LogFrame(std::move(dict));

  enabled_.store(true);
  read_write_lock_.WriteUnLock();
}

void VizDebugger::StopDebugStream() {
  read_write_lock_.WriteLock();
  DCHECK(gpu_thread_task_runner_->RunsTasksInCurrentSequence());
  debug_output_.reset();
  enabled_.store(false);
  read_write_lock_.WriteUnLock();
}

void VizDebugger::AddLogMessage(std::string log,
                                const VizDebugger::StaticSource* dcs,
                                DrawOption option) {
  //  Store atomic insertion index in local variable to use to insert into
  //  buffer.
  int insertion_index;

  for (;;) {
    read_write_lock_.ReadLock();
    // Get call insertion index.
    insertion_index = logs_tail_idx_++;
    // If the insertion index is within bounds, insert call into buffer.
    if (static_cast<size_t>(insertion_index) < logs_.size()) {
      int cur_thread_id = base::PlatformThread::CurrentId();
      logs_[insertion_index] = LogCall{submission_count_++, dcs->reg_index,
                                       cur_thread_id, option, std::move(log)};
      // Return when call insertion is successful.
      read_write_lock_.ReadUnlock();
      return;
    }
    read_write_lock_.ReadUnlock();
    // Take write lock to resize and re-adjust buffer tail index after buffer
    // overflow.
    read_write_lock_.WriteLock();
    // If tail index is over buffer size, then resizing is definitely needed.
    // Also re-adjust tail index so it's at the start of the new buffer space.
    if (static_cast<size_t>(logs_tail_idx_) >= logs_.size()) {
      logs_tail_idx_ = logs_.size();
      logs_.resize(logs_.size() * 2);
    }
    read_write_lock_.WriteUnLock();
  }
}

}  // namespace viz

#endif  // VIZ_DEBUGGER_IS_ON()
