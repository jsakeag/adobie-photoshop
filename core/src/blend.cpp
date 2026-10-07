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
  switch (mode) {
    case BlendMode::Normal:
      return normal(dst, src);
  }
  return dst;
}

}  // namespace ps
