// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cros_audio_config_impl.h"

#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash::audio_config {

mojom::AudioDeviceType ComputeDeviceType(const AudioDeviceType& device_type) {
  switch (device_type) {
    case AudioDeviceType::kHeadphone:
      return mojom::AudioDeviceType::kHeadphone;
    case AudioDeviceType::kMic:
      return mojom::AudioDeviceType::kMic;
    case AudioDeviceType::kUsb:
      return mojom::AudioDeviceType::kUsb;
    case AudioDeviceType::kBluetooth:
      return mojom::AudioDeviceType::kBluetooth;
    case AudioDeviceType::kBluetoothNbMic:
      return mojom::AudioDeviceType::kBluetoothNbMic;
    case AudioDeviceType::kHdmi:
      return mojom::AudioDeviceType::kHdmi;
    case AudioDeviceType::kInternalSpeaker:
      return mojom::AudioDeviceType::kInternalSpeaker;
    case AudioDeviceType::kInternalMic:
      return mojom::AudioDeviceType::kInternalMic;
    case AudioDeviceType::kFrontMic:
      return mojom::AudioDeviceType::kFrontMic;
    case AudioDeviceType::kRearMic:
      return mojom::AudioDeviceType::kRearMic;
    case AudioDeviceType::kKeyboardMic:
      return mojom::AudioDeviceType::kKeyboardMic;
    case AudioDeviceType::kHotword:
      return mojom::AudioDeviceType::kHotword;
    case AudioDeviceType::kPostDspLoopback:
      return mojom::AudioDeviceType::kPostDspLoopback;
    case AudioDeviceType::kPostMixLoopback:
      return mojom::AudioDeviceType::kPostMixLoopback;
    case AudioDeviceType::kLineout:
      return mojom::AudioDeviceType::kLineout;
    case AudioDeviceType::kAlsaLoopback:
      return mojom::AudioDeviceType::kAlsaLoopback;
    case AudioDeviceType::kOther:
      return mojom::AudioDeviceType::kOther;
  };
}

mojom::AudioDevicePtr GenerateMojoAudioDevice(const AudioDevice& device) {
  mojom::AudioDevicePtr mojo_device = mojom::AudioDevice::New();
  mojo_device->id = device.id;
  mojo_device->display_name = device.display_name;
  mojo_device->is_active = device.active;
  mojo_device->device_type = ComputeDeviceType(device.type);
  return mojo_device;
}

CrosAudioConfigImpl::CrosAudioConfigImpl() {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

CrosAudioConfigImpl::~CrosAudioConfigImpl() {
  if (CrasAudioHandler::Get())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

uint8_t CrosAudioConfigImpl::GetOutputVolumePercent() const {
  return CrasAudioHandler::Get()->GetOutputVolumePercent();
}

mojom::MuteState CrosAudioConfigImpl::GetOutputMuteState() const {
  // TODO(crbug.com/1092970): Add kMutedExternally.
  if (CrasAudioHandler::Get()->IsOutputMutedByPolicy())
    return mojom::MuteState::kMutedByPolicy;

  if (CrasAudioHandler::Get()->IsOutputMuted())
    return mojom::MuteState::kMutedByUser;

  return mojom::MuteState::kNotMuted;
}

void CrosAudioConfigImpl::GetAudioDevices(
    std::vector<mojom::AudioDevicePtr>* output_devices_out) const {
  AudioDeviceList audio_devices_list;
  CrasAudioHandler::Get()->GetAudioDevices(&audio_devices_list);
  for (const auto& device : audio_devices_list) {
    if (!device.is_for_simple_usage()) {
      continue;
    }

    // TODO(crbug.com/1092970): Add input_devices.
    if (!device.is_input) {
      output_devices_out->push_back(GenerateMojoAudioDevice(device));
    }
  }
}

void CrosAudioConfigImpl::SetOutputVolumePercent(int8_t volume) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->SetOutputVolumePercent(volume);

  // If the volume is above certain level and it's muted, it should be unmuted.
  if (audio_handler->IsOutputMuted() &&
      volume > audio_handler->GetOutputDefaultVolumeMuteThreshold()) {
    audio_handler->SetOutputMute(false);
  }
}

void CrosAudioConfigImpl::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                    int volume) {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnOutputMuteChanged(bool mute_on) {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnAudioNodesChanged() {
  NotifyObserversAudioSystemPropertiesChanged();
}

}  // namespace ash::audio_config
