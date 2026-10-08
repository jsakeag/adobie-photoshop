#pragma once

#include "ps/image.h"

namespace ps {

enum class BlendMode {
  Normal,
  Multiply,
  Screen,
  Overlay,
  SoftLight,
  HardLight,
  Darken,
  Lighten,
  Difference,
  ColorDodge,
  ColorBurn,
};

// Premultiplied RGBA with channels in 0..1: the compositor's working format.
struct Premul {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 0.0f;
};

Premul premultiply(Color c);
// Rounds to the nearest 8-bit value. Zero alpha gives kTransparent.
Color unpremultiply(Premul p);

// Composites src onto dst. Opacity and mask are applied to src beforehand.
// The mode only acts where both pixels have coverage; elsewhere the result
// is the same as Normal.
Premul blend(BlendMode mode, Premul dst, Premul src);

}  // namespace ps
