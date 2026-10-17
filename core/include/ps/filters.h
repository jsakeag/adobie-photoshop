#pragma once

#include <cstdint>
#include <variant>

#include "ps/geometry.h"
#include "ps/image.h"

namespace ps {

// The Filter menu. Blurs work on premultiplied colour, so transparent
// neighbours soften alpha without darkening colour.

struct GaussianBlur {
  float radius = 2.0f;  // standard deviation in pixels; 0 does nothing
};

// Sharpens by adding back the difference from a Gaussian blur, where that
// difference is at least `threshold` levels.
struct UnsharpMask {
  float amount = 1.0f;  // 1 = 100%
  float radius = 1.0f;
  int threshold = 0;
};

// Adds uniform noise of up to `amount` levels to each colour channel. The
// noise depends only on the seed and the document position, so the same
// command gives the same pixels wherever and however often it runs.
struct AddNoise {
  int amount = 25;
  bool monochrome = false;  // the same offset on all three channels
  std::uint32_t seed = 1;
};

// Averages `distance` pixels along a line at `angle` degrees
// (counter-clockwise from horizontal).
struct MotionBlur {
  float angle = 0.0f;
  int distance = 10;
};

using Filter = std::variant<GaussianBlur, UnsharpMask, AddNoise, MotionBlur>;

// "Gaussian Blur", "Unsharp Mask", "Add Noise" or "Motion Blur".
const char* filter_name(const Filter& filter);

// How far, in pixels, the filter reads around each pixel it changes.
int filter_halo(const Filter& filter);

// Filters `source`, whose top-left is at document position `origin`.
// Pixels within filter_halo() of its edges have incomplete context: callers
// pass a margin of that width and keep only the inside.
Image apply_filter(const Filter& filter, const Image& source, Point origin);

}  // namespace ps
