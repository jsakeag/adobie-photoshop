#pragma once

#include <cstddef>
#include <memory>
#include <variant>

#include "ps/adjust_commands.h"
#include "ps/filter_commands.h"

// Everything the Adjustments & Filters palette offers, as one list: the
// adjustments first, then the filters. The core keeps them apart
// (ps::Adjustment, ps::Filter); this grouping is only for the palette.
using Effect = std::variant<ps::Levels, ps::Curves, ps::BrightnessContrast,
                            ps::HueSaturation, ps::Invert, ps::GaussianBlur,
                            ps::UnsharpMask, ps::AddNoise, ps::MotionBlur>;

inline constexpr std::size_t kEffectCount = std::variant_size_v<Effect>;
// Kinds below this are adjustments, the rest filters.
inline constexpr std::size_t kFirstFilter = 5;

// A fresh effect of each kind, with neutral settings where it has any.
inline Effect default_effect(std::size_t kind) {
  switch (kind) {
    case 0: return ps::Levels{};
    case 1: return ps::Curves{{{0, 0}, {64, 64}, {128, 128}, {192, 192}, {255, 255}}};
    case 2: return ps::BrightnessContrast{};
    case 3: return ps::HueSaturation{};
    case 4: return ps::Invert{};
    case 5: return ps::GaussianBlur{0.0f};
    case 6: return ps::UnsharpMask{0.0f, 1.0f, 0};
    case 7: return ps::AddNoise{0, false, 1};
    default: return ps::MotionBlur{0.0f, 1};
  }
}

inline const char* effect_name(std::size_t kind) {
  static const char* const names[kEffectCount] = {
      "Levels",       "Curves",       "Brightness/Contrast",
      "Hue/Saturation", "Invert",     "Gaussian Blur",
      "Unsharp Mask", "Add Noise",    "Motion Blur"};
  return kind < kEffectCount ? names[kind] : "";
}

// The command that applies `effect` to a layer.
inline std::unique_ptr<ps::Command> effect_command(std::size_t layer,
                                                   const Effect& effect) {
  return std::visit(
      [layer](const auto& e) -> std::unique_ptr<ps::Command> {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_constructible_v<ps::Adjustment, T>) {
          return std::make_unique<ps::AdjustCommand>(layer, e);
        } else {
          return std::make_unique<ps::FilterCommand>(layer, e);
        }
      },
      effect);
}
