// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_programmable_stage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

namespace blink {

WGPUComputePipelineDescriptor AsDawnType(
    GPUDevice* device,
    const GPUComputePipelineDescriptor* webgpu_desc,
    std::string* label,
    OwnedProgrammableStageDescriptor* computeStageDescriptor) {
  DCHECK(webgpu_desc);
  DCHECK(label);
  DCHECK(computeStageDescriptor);

  WGPUComputePipelineDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;

  if (webgpu_desc->hasLayout()) {
    dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  } else {
    // TODO(crbug.com/1069302): Remove this branch after the deprecation period.
    device->AddConsoleWarning(
        "Leaving the layout of a compute pipeline undefined has been "
        "deprecated, and specifying a pipeline layout will soon be required. "
        "Set layout to 'auto' if an implicit pipeline layout is desired.");
  }

  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  *computeStageDescriptor = AsDawnType(webgpu_desc->compute());
  dawn_desc.compute = std::get<0>(*computeStageDescriptor);

  return dawn_desc;
}

// static
GPUComputePipeline* GPUComputePipeline::Create(
    GPUDevice* device,
    const GPUComputePipelineDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  OwnedProgrammableStageDescriptor computeStageDescriptor;
  WGPUComputePipelineDescriptor dawn_desc =
      AsDawnType(device, webgpu_desc, &label, &computeStageDescriptor);

  GPUComputePipeline* pipeline = MakeGarbageCollected<GPUComputePipeline>(
      device, device->GetProcs().deviceCreateComputePipeline(
                  device->GetHandle(), &dawn_desc));
  if (webgpu_desc->hasLabel())
    pipeline->setLabel(webgpu_desc->label());
  return pipeline;
}

GPUComputePipeline::GPUComputePipeline(GPUDevice* device,
                                       WGPUComputePipeline compute_pipeline)
    : DawnObject<WGPUComputePipeline>(device, compute_pipeline) {}

GPUBindGroupLayout* GPUComputePipeline::getBindGroupLayout(uint32_t index) {
  return MakeGarbageCollected<GPUBindGroupLayout>(
      device_,
      GetProcs().computePipelineGetBindGroupLayout(GetHandle(), index));
}

}  // namespace blink
