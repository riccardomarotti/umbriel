#pragma once

#include "scene/effect_input_mapping.h"

#include <array>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" {
#include <umbrielfx/render/effect.h>
}

namespace umbriel {

  // Reserve room in fx_animation_parameters for time, palette, palette_count
  // and the compositor-owned availability uniform (FX_UNIFORMS_MAX == 8).
  inline constexpr size_t kMaxLiveEffectUniforms = 4;

  // Diagnostic-only metadata. All timestamps are CLOCK_MONOTONIC nanoseconds;
  // receivedMonotonicNs is stamped by the compositor, never trusted from IPC.
  struct EffectLatencyProbe {
    uint64_t sequence = 0;
    uint64_t sampleMonotonicNs = 0;
    uint64_t sentMonotonicNs = 0;
    uint64_t receivedMonotonicNs = 0;
  };

  // A complete output-scoped snapshot. Producers assign meaning to the names;
  // the compositor only validates their GLSL types and forwards their values.
  struct LiveEffectUniforms {
    std::string output;
    std::array<fx_uniform, kMaxLiveEffectUniforms> uniforms{};
    size_t count = 0;
    std::optional<EffectInputMapping> inputMapping;
    std::optional<EffectLatencyProbe> latencyProbe;
  };

} // namespace umbriel
