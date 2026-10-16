#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <variant>
#include <vector>

#include "ps/geometry.h"
#include "ps/image.h"

namespace ps {

// Image > Adjust. Each works on red, green and blue and leaves alpha alone.

// Maps input [input_black, input_white] to output [output_black,
// output_white] through a gamma curve (gamma > 1 brightens midtones).
struct Levels {
  int input_black = 0;
  int input_white = 255;
  float gamma = 1.0f;
  int output_black = 0;
  int output_white = 255;
};

// A smooth curve through (input, output) points in 0..255, monotone
// between points (no overshoot) and flat beyond the first and last.
struct Curves {
  std::vector<Point> points{{0, 0}, {255, 255}};
};

// Brightness shifts every level by that many (-100..100); contrast then
// stretches levels around the middle (-100 flattens to grey, 100 is a
// threshold).
struct BrightnessContrast {
  int brightness = 0;
  int contrast = 0;
};

// Hue rotates colours (-180..180 degrees); saturation scales it
// (-100 = grey, 100 = double); lightness moves toward white or black
// (-100..100).
struct HueSaturation {
  int hue = 0;
  int saturation = 0;
  int lightness = 0;
};

struct Invert {};

using Adjustment =
    std::variant<Levels, Curves, BrightnessContrast, HueSaturation, Invert>;

// "Levels", "Curves", "Brightness/Contrast", "Hue/Saturation" or "Invert".
const char* adjustment_name(const Adjustment& adjustment);

// The adjustments that act on each channel level independently, as lookup
// tables from input level to output level.
using Lut = std::array<std::uint8_t, 256>;
Lut levels_lut(const Levels& levels);
Lut curves_lut(const Curves& curves);
Lut brightness_contrast_lut(const BrightnessContrast& bc);
Lut invert_lut();

Color adjust_hue_saturation(Color c, const HueSaturation& hs);

// The adjustment's effect on one pixel.
std::function<Color(Color)> adjustment_function(const Adjustment& adjustment);

}  // namespace ps
