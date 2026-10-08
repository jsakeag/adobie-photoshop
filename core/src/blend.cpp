#include "ps/blend.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ps {

namespace {

std::uint8_t to_byte(float v) {
  return static_cast<std::uint8_t>(
      std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}

Premul normal(Premul dst, Premul src) {
  const float keep = 1.0f - src.a;
  return {src.r + dst.r * keep, src.g + dst.g * keep, src.b + dst.b * keep,
          src.a + dst.a * keep};
}

// The per-channel blend functions below take straight (unpremultiplied)
// values in 0..1: b is the backdrop, s the source.

float multiply(float b, float s) { return b * s; }

float screen(float b, float s) { return b + s - b * s; }

float hard_light(float b, float s) {
  return s <= 0.5f ? 2.0f * b * s : 1.0f - 2.0f * (1.0f - b) * (1.0f - s);
}

float overlay(float b, float s) { return hard_light(s, b); }

float soft_light(float b, float s) {
  return s <= 0.5f ? 2.0f * b * s + b * b * (1.0f - 2.0f * s)
                   : 2.0f * b * (1.0f - s) + std::sqrt(b) * (2.0f * s - 1.0f);
}

float darken(float b, float s) { return std::min(b, s); }

float lighten(float b, float s) { return std::max(b, s); }

float difference(float b, float s) { return std::fabs(b - s); }

float color_dodge(float b, float s) {
  if (b <= 0.0f) return 0.0f;
  if (s >= 1.0f) return 1.0f;
  return std::min(1.0f, b / (1.0f - s));
}

float color_burn(float b, float s) {
  if (b >= 1.0f) return 1.0f;
  if (s <= 0.0f) return 0.0f;
  return 1.0f - std::min(1.0f, (1.0f - b) / s);
}

using ChannelBlend = float (*)(float, float);

ChannelBlend channel_blend(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal: return nullptr;
    case BlendMode::Multiply: return multiply;
    case BlendMode::Screen: return screen;
    case BlendMode::Overlay: return overlay;
    case BlendMode::SoftLight: return soft_light;
    case BlendMode::HardLight: return hard_light;
    case BlendMode::Darken: return darken;
    case BlendMode::Lighten: return lighten;
    case BlendMode::Difference: return difference;
    case BlendMode::ColorDodge: return color_dodge;
    case BlendMode::ColorBurn: return color_burn;
  }
  return nullptr;
}

}  // namespace

Premul premultiply(Color c) {
  const float a = c.a / 255.0f;
  return {c.r / 255.0f * a, c.g / 255.0f * a, c.b / 255.0f * a, a};
}

Color unpremultiply(Premul p) {
  const std::uint8_t a = to_byte(p.a);
  if (a == 0) return kTransparent;
  return {to_byte(p.r / p.a), to_byte(p.g / p.a), to_byte(p.b / p.a), a};
}

Premul blend(BlendMode mode, Premul dst, Premul src) {
  const ChannelBlend mix = channel_blend(mode);
  if (!mix || src.a <= 0.0f || dst.a <= 0.0f) return normal(dst, src);

  // Source alone where the backdrop is clear, backdrop alone where the
  // source is clear, and the blend function where they overlap.
  const float both = src.a * dst.a;
  const float src_only = 1.0f - dst.a;
  const float dst_only = 1.0f - src.a;
  const auto channel = [&](float d, float s) {
    const float b = std::min(d / dst.a, 1.0f);
    const float c = std::min(s / src.a, 1.0f);
    return s * src_only + d * dst_only + both * mix(b, c);
  };
  return {channel(dst.r, src.r), channel(dst.g, src.g), channel(dst.b, src.b),
          src.a + dst.a - both};
}

}  // namespace ps
