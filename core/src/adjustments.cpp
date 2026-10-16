#include "ps/adjustments.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ps {

namespace {

std::uint8_t to_level(double v) {
  return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
}

std::function<Color(Color)> from_lut(const Lut& lut) {
  return [lut](Color c) { return Color{lut[c.r], lut[c.g], lut[c.b], c.a}; };
}

double hue_to_channel(double p, double q, double t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  if (t < 1.0 / 6) return p + (q - p) * 6 * t;
  if (t < 1.0 / 2) return q;
  if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
  return p;
}

}  // namespace

const char* adjustment_name(const Adjustment& adjustment) {
  switch (adjustment.index()) {
    case 0: return "Levels";
    case 1: return "Curves";
    case 2: return "Brightness/Contrast";
    case 3: return "Hue/Saturation";
    case 4: return "Invert";
  }
  return "";
}

Lut levels_lut(const Levels& levels) {
  Lut lut;
  const double span = std::max(1, levels.input_white - levels.input_black);
  const double gamma = std::max(0.01f, levels.gamma);
  for (int v = 0; v < 256; ++v) {
    double t = std::clamp((v - levels.input_black) / span, 0.0, 1.0);
    t = std::pow(t, 1.0 / gamma);
    lut[v] = to_level(levels.output_black +
                      t * (levels.output_white - levels.output_black));
  }
  return lut;
}

Lut curves_lut(const Curves& curves) {
  // Sorted points with distinct inputs; a later point wins a tie.
  std::vector<Point> p;
  for (Point q : curves.points) {
    q = {std::clamp(q.x, 0, 255), std::clamp(q.y, 0, 255)};
    auto at = std::find_if(p.begin(), p.end(), [&](Point o) { return o.x == q.x; });
    if (at != p.end()) {
      *at = q;
    } else {
      p.push_back(q);
    }
  }
  std::sort(p.begin(), p.end(), [](Point a, Point b) { return a.x < b.x; });

  Lut lut;
  if (p.size() < 2) {
    for (int v = 0; v < 256; ++v) lut[v] = static_cast<std::uint8_t>(v);
    return lut;
  }

  // Monotone cubic Hermite (Fritsch-Carlson): tangents from the secants,
  // flattened at extremes and limited so no segment overshoots.
  const std::size_t n = p.size();
  std::vector<double> secant(n - 1), tangent(n);
  for (std::size_t k = 0; k + 1 < n; ++k) {
    secant[k] = double(p[k + 1].y - p[k].y) / (p[k + 1].x - p[k].x);
  }
  tangent[0] = secant[0];
  tangent[n - 1] = secant[n - 2];
  for (std::size_t k = 1; k + 1 < n; ++k) {
    tangent[k] = secant[k - 1] * secant[k] <= 0 ? 0.0
                                                 : (secant[k - 1] + secant[k]) / 2;
  }
  for (std::size_t k = 0; k + 1 < n; ++k) {
    if (secant[k] == 0) {
      tangent[k] = tangent[k + 1] = 0;
      continue;
    }
    const double a = tangent[k] / secant[k], b = tangent[k + 1] / secant[k];
    const double s = a * a + b * b;
    if (s > 9) {
      const double t = 3 / std::sqrt(s);
      tangent[k] = t * a * secant[k];
      tangent[k + 1] = t * b * secant[k];
    }
  }

  std::size_t k = 0;
  for (int v = 0; v < 256; ++v) {
    if (v <= p.front().x) {
      lut[v] = static_cast<std::uint8_t>(p.front().y);
      continue;
    }
    if (v >= p.back().x) {
      lut[v] = static_cast<std::uint8_t>(p.back().y);
      continue;
    }
    while (v > p[k + 1].x) ++k;
    const double h = p[k + 1].x - p[k].x;
    const double t = (v - p[k].x) / h;
    const double t2 = t * t, t3 = t2 * t;
    lut[v] = to_level((2 * t3 - 3 * t2 + 1) * p[k].y +
                      (t3 - 2 * t2 + t) * h * tangent[k] +
                      (-2 * t3 + 3 * t2) * p[k + 1].y +
                      (t3 - t2) * h * tangent[k + 1]);
  }
  return lut;
}

Lut brightness_contrast_lut(const BrightnessContrast& bc) {
  const double c = std::clamp(bc.contrast, -100, 100) / 100.0;
  // Contrast 100 would be an infinite stretch: cap it at a threshold.
  const double factor = c >= 0 ? 1.0 / std::max(1.0 - c, 1.0 / 512) : 1.0 + c;
  Lut lut;
  for (int v = 0; v < 256; ++v) {
    const double bright = std::clamp(v + std::clamp(bc.brightness, -100, 100), 0, 255);
    lut[v] = to_level((bright - 127.5) * factor + 127.5);
  }
  return lut;
}

Lut invert_lut() {
  Lut lut;
  for (int v = 0; v < 256; ++v) lut[v] = static_cast<std::uint8_t>(255 - v);
  return lut;
}

Color adjust_hue_saturation(Color c, const HueSaturation& hs) {
  // RGB to HSL.
  const double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
  const double hi = std::max({r, g, b}), lo = std::min({r, g, b});
  double l = (hi + lo) / 2, h = 0, s = 0;
  if (hi > lo) {
    const double d = hi - lo;
    s = l > 0.5 ? d / (2 - hi - lo) : d / (hi + lo);
    if (hi == r) {
      h = (g - b) / d + (g < b ? 6 : 0);
    } else if (hi == g) {
      h = (b - r) / d + 2;
    } else {
      h = (r - g) / d + 4;
    }
    h /= 6;
  }

  h = std::fmod(h + std::clamp(hs.hue, -180, 180) / 360.0 + 1.0, 1.0);
  s = std::clamp(s * (1 + std::clamp(hs.saturation, -100, 100) / 100.0), 0.0, 1.0);
  const double light = std::clamp(hs.lightness, -100, 100) / 100.0;
  l = light >= 0 ? l + (1 - l) * light : l * (1 + light);

  // HSL to RGB.
  if (s == 0) {
    const std::uint8_t v = to_level(l * 255);
    return {v, v, v, c.a};
  }
  const double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  const double p = 2 * l - q;
  return {to_level(hue_to_channel(p, q, h + 1.0 / 3) * 255),
          to_level(hue_to_channel(p, q, h) * 255),
          to_level(hue_to_channel(p, q, h - 1.0 / 3) * 255), c.a};
}

std::function<Color(Color)> adjustment_function(const Adjustment& adjustment) {
  switch (adjustment.index()) {
    case 0: return from_lut(levels_lut(std::get<Levels>(adjustment)));
    case 1: return from_lut(curves_lut(std::get<Curves>(adjustment)));
    case 2:
      return from_lut(brightness_contrast_lut(std::get<BrightnessContrast>(adjustment)));
    case 3: {
      const HueSaturation hs = std::get<HueSaturation>(adjustment);
      return [hs](Color c) { return adjust_hue_saturation(c, hs); };
    }
    default: return from_lut(invert_lut());
  }
}

}  // namespace ps
