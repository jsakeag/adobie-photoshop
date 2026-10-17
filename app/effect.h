#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <variant>

#include "ps/adjust_commands.h"
#include "ps/canvas_commands.h"
#include "ps/filter_commands.h"
#include "ps/transform_commands.h"

// Settings for the Transform menu, as the palette shows them. The commands
// take pixels and matrices; these are converted when the command is made.
struct ImageSizeEffect {
  float percent = 100.0f;  // of the current size, aspect kept
};
struct CanvasSizeEffect {
  float width_percent = 100.0f;
  float height_percent = 100.0f;
  int anchor = 4;  // ps::Anchor, 4 = centre
};
struct LayerTransformEffect {
  float scale_x = 100.0f;  // percent
  float scale_y = 100.0f;
  float angle = 0.0f;      // degrees clockwise
};

// Everything the Image palette offers, as one list: adjustments, filters,
// then transforms. The core keeps them apart; this grouping is only for the
// palette.
using Effect = std::variant<ps::Levels, ps::Curves, ps::BrightnessContrast,
                            ps::HueSaturation, ps::Invert, ps::GaussianBlur,
                            ps::UnsharpMask, ps::AddNoise, ps::MotionBlur,
                            ImageSizeEffect, CanvasSizeEffect, LayerTransformEffect>;

inline constexpr std::size_t kEffectCount = std::variant_size_v<Effect>;

// The palette's menus, each a run of kinds.
struct EffectMenu {
  const char* placeholder;
  std::size_t first;
  std::size_t count;
};
inline constexpr std::array<EffectMenu, 3> kEffectMenus = {{
    {"Adjust...", 0, 5},
    {"Filter...", 5, 4},
    {"Transform...", 9, 3},
}};

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
    case 8: return ps::MotionBlur{0.0f, 1};
    case 9: return ImageSizeEffect{};
    case 10: return CanvasSizeEffect{};
    default: return LayerTransformEffect{};
  }
}

inline const char* effect_name(std::size_t kind) {
  static const char* const names[kEffectCount] = {
      "Levels",       "Curves",      "Brightness/Contrast", "Hue/Saturation",
      "Invert",       "Gaussian Blur", "Unsharp Mask",      "Add Noise",
      "Motion Blur",  "Image Size",  "Canvas Size",         "Scale/Rotate Layer"};
  return kind < kEffectCount ? names[kind] : "";
}

// The command that applies `effect` to a layer of `doc`. Previews resample
// bilinearly for speed; the real edit uses bicubic.
inline std::unique_ptr<ps::Command> effect_command(std::size_t layer,
                                                   const Effect& effect,
                                                   const ps::Document& doc,
                                                   bool preview) {
  const auto interpolation =
      preview ? ps::Interpolation::Bilinear : ps::Interpolation::Bicubic;
  const auto percent_of = [](int size, float percent) {
    return std::max(1, static_cast<int>(std::lround(size * percent / 100.0f)));
  };
  return std::visit(
      [&](const auto& e) -> std::unique_ptr<ps::Command> {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, ImageSizeEffect>) {
          return std::make_unique<ps::ImageSizeCommand>(
              percent_of(doc.width(), e.percent), percent_of(doc.height(), e.percent),
              interpolation);
        } else if constexpr (std::is_same_v<T, CanvasSizeEffect>) {
          return std::make_unique<ps::ResizeCanvasCommand>(
              percent_of(doc.width(), e.width_percent),
              percent_of(doc.height(), e.height_percent),
              static_cast<ps::Anchor>(std::clamp(e.anchor, 0, 8)), ps::kWhite);
        } else if constexpr (std::is_same_v<T, LayerTransformEffect>) {
          // About the centre of what the layer shows, or of the canvas.
          ps::Vec2 centre{doc.width() / 2.0, doc.height() / 2.0};
          if (layer < doc.layers().size()) {
            const ps::Layer& l = doc.layers().at(layer);
            const ps::Rect shown = ps::opaque_bounds(l.pixels);
            if (!shown.empty()) {
              centre = {l.offset.x + shown.x + shown.width / 2.0,
                        l.offset.y + shown.y + shown.height / 2.0};
            }
          }
          return std::make_unique<ps::TransformLayerCommand>(
              layer,
              ps::scale_rotate_about(centre, e.scale_x / 100.0, e.scale_y / 100.0, e.angle),
              interpolation);
        } else if constexpr (std::is_constructible_v<ps::Adjustment, T>) {
          return std::make_unique<ps::AdjustCommand>(layer, e);
        } else {
          return std::make_unique<ps::FilterCommand>(layer, e);
        }
      },
      effect);
}
