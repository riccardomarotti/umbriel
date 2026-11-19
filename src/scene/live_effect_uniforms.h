#pragma once

#include "scene/effect_input_mapping.h"

#include <array>
#include <optional>
#include <cstddef>
#include <string>

extern "C" {
#include <umbrielfx/render/effect.h>
}

namespace umbriel {

  // Reserve room in fx_animation_parameters for time, palette, palette_count
  // and the compositor-owned availability uniform (FX_UNIFORMS_MAX == 8).
  inline constexpr size_t kMaxLiveEffectUniforms = 4;

  // A complete output-scoped snapshot. Producers assign meaning to the names;
  // the compositor only validates their GLSL types and forwards their values.
  struct LiveEffectUniforms {
    std::string output;
    std::array<fx_uniform, kMaxLiveEffectUniforms> uniforms{};
    size_t count = 0;
    std::optional<EffectInputMapping> inputMapping;
  };

} // namespace umbriel
