// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_ASH_PROXY_H_
#define REMOTING_HOST_CHROMEOS_ASH_PROXY_H_

#include <cstdint>
#include <vector>

#include "base/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"

class SkBitmap;

namespace viz {
class FrameSinkId;
namespace mojom {
class FrameSinkVideoCapturer;
}  // namespace mojom
}  // namespace viz

namespace remoting {

using DisplayId = int64_t;

// Utility proxy class that abstracts away all ash related actions on ChromeOs,
// to prevent our code from directly calling `ash::Shell` so we can mock things
// during unittests.
class AshProxy {
 public:
  static AshProxy& Get();

  // The caller is responsible to ensure this given instance lives long enough.
  // To unset call this method again with nullptr.
  static void SetInstanceForTesting(AshProxy* instance);

  // Convert the scale factor to DPI.
  static int ScaleFactorToDpi(float scale_factor);
  static int GetDpi(const display::Display& display);

  virtual ~AshProxy();

  virtual DisplayId GetPrimaryDisplayId() const = 0;

  virtual const std::vector<display::Display>& GetActiveDisplays() const = 0;

  virtual const display::Display* GetDisplayForId(
      DisplayId display_id) const = 0;

  using ScreenshotCallback = base::OnceCallback<void(absl::optional<SkBitmap>)>;
  virtual void TakeScreenshotOfDisplay(DisplayId display_id,
                                       ScreenshotCallback callback) = 0;

  virtual void CreateVideoCapturer(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
          video_capturer) = 0;

  virtual viz::FrameSinkId GetFrameSinkId(DisplayId source_display_id) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_ASH_PROXY_H_
