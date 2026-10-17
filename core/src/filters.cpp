#include "ps/filters.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "ps/blend.h"

namespace ps {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Premultiplied float pixels, row by row, for filters that mix neighbours.
struct Buffer {
  int width = 0;
  int height = 0;
  std::vector<Premul> px;

  Premul& at(int x, int y) { return px[static_cast<std::size_t>(y) * width + x]; }
  const Premul& clamped(int x, int y) const {
    x = std::clamp(x, 0, width - 1);
    y = std::clamp(y, 0, height - 1);
    return px[static_cast<std::size_t>(y) * width + x];
  }
};

Buffer to_buffer(const Image& image) {
  Buffer b{image.width(), image.height(), {}};
  b.px.resize(static_cast<std::size_t>(b.width) * b.height);
  const std::uint8_t* p = image.data();
  for (Premul& out : b.px) {
    out = premultiply({p[0], p[1], p[2], p[3]});
    p += Image::kChannels;
  }
  return b;
}

Image to_image(const Buffer& b) {
  Image image(b.width, b.height);
  std::uint8_t* p = image.data();
  for (const Premul& in : b.px) {
    const Color c = unpremultiply(in);
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
    p[3] = c.a;
    p += Image::kChannels;
  }
  return image;
}

void add_scaled(Premul& sum, const Premul& p, float w) {
  sum.r += p.r * w;
  sum.g += p.g * w;
  sum.b += p.b * w;
  sum.a += p.a * w;
}

int gaussian_reach(float radius) {
  return static_cast<int>(std::ceil(3.0f * radius));
}

Image gaussian(const Image& source, float radius) {
  if (radius <= 0.0f || source.empty()) return source;
  const int reach = gaussian_reach(radius);
  std::vector<float> kernel(2 * reach + 1);
  float total = 0.0f;
  for (int k = -reach; k <= reach; ++k) {
    total += kernel[k + reach] = std::exp(-0.5f * k * k / (radius * radius));
  }
  for (float& w : kernel) w /= total;

  Buffer in = to_buffer(source);
  Buffer across = in;
  for (int y = 0; y < in.height; ++y) {
    for (int x = 0; x < in.width; ++x) {
      Premul sum;
      for (int k = -reach; k <= reach; ++k) {
        add_scaled(sum, in.clamped(x + k, y), kernel[k + reach]);
      }
      across.at(x, y) = sum;
    }
  }
  for (int y = 0; y < in.height; ++y) {
    for (int x = 0; x < in.width; ++x) {
      Premul sum;
      for (int k = -reach; k <= reach; ++k) {
        add_scaled(sum, across.clamped(x, y + k), kernel[k + reach]);
      }
      in.at(x, y) = sum;
    }
  }
  return to_image(in);
}

Image unsharp(const Image& source, const UnsharpMask& f) {
  const Image blurred = gaussian(source, f.radius);
  Image out = source;
  const std::uint8_t* b = blurred.data();
  std::uint8_t* p = out.data();
  for (std::size_t i = 0; i < out.size_bytes(); i += Image::kChannels) {
    if (p[i + 3] == 0) continue;
    for (int ch = 0; ch < 3; ++ch) {
      const int diff = p[i + ch] - b[i + ch];
      if (std::abs(diff) < f.threshold) continue;
      p[i + ch] = static_cast<std::uint8_t>(
          std::clamp<long>(std::lround(p[i + ch] + f.amount * diff), 0, 255));
    }
  }
  return out;
}

// A well-mixed 64-bit hash (splitmix64's finaliser).
std::uint64_t mix(std::uint64_t z) {
  z += 0x9e3779b97f4a7c15ULL;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

Image noise(const Image& source, const AddNoise& f, Point origin) {
  Image out = source;
  const int amount = std::clamp(f.amount, 0, 255);
  if (amount == 0) return out;
  const std::uint64_t span = 2 * static_cast<std::uint64_t>(amount) + 1;
  for (int y = 0; y < out.height(); ++y) {
    for (int x = 0; x < out.width(); ++x) {
      std::uint8_t* p = out.data() + (static_cast<std::size_t>(y) * out.width() + x) * 4;
      if (p[3] == 0) continue;
      const std::uint64_t at =
          mix((static_cast<std::uint64_t>(f.seed) << 40) ^
              (static_cast<std::uint64_t>(static_cast<std::uint32_t>(origin.x + x)) << 20) ^
              static_cast<std::uint32_t>(origin.y + y));
      for (int ch = 0; ch < 3; ++ch) {
        const std::uint64_t h = mix(at + static_cast<std::uint64_t>(f.monochrome ? 0 : ch));
        const int offset = static_cast<int>(h % span) - amount;
        p[ch] = static_cast<std::uint8_t>(std::clamp(p[ch] + offset, 0, 255));
      }
    }
  }
  return out;
}

Image motion(const Image& source, const MotionBlur& f) {
  const int n = std::max(1, f.distance);
  if (n == 1 || source.empty()) return source;
  const double angle = f.angle * kPi / 180.0;
  std::vector<std::pair<int, int>> steps;
  for (int k = 0; k < n; ++k) {
    const double t = k - (n - 1) / 2.0;
    steps.emplace_back(static_cast<int>(std::lround(t * std::cos(angle))),
                       static_cast<int>(std::lround(-t * std::sin(angle))));
  }
  const Buffer in = to_buffer(source);
  Buffer out = in;
  const float w = 1.0f / n;
  for (int y = 0; y < in.height; ++y) {
    for (int x = 0; x < in.width; ++x) {
      Premul sum;
      for (const auto& [dx, dy] : steps) add_scaled(sum, in.clamped(x + dx, y + dy), w);
      out.at(x, y) = sum;
    }
  }
  return to_image(out);
}

}  // namespace

const char* filter_name(const Filter& filter) {
  switch (filter.index()) {
    case 0: return "Gaussian Blur";
    case 1: return "Unsharp Mask";
    case 2: return "Add Noise";
    case 3: return "Motion Blur";
  }
  return "";
}

int filter_halo(const Filter& filter) {
  switch (filter.index()) {
    case 0: return gaussian_reach(std::get<GaussianBlur>(filter).radius) + 1;
    case 1: return gaussian_reach(std::get<UnsharpMask>(filter).radius) + 1;
    case 2: return 0;
    case 3: return (std::max(1, std::get<MotionBlur>(filter).distance) + 1) / 2 + 1;
  }
  return 0;
}

Image apply_filter(const Filter& filter, const Image& source, Point origin) {
  switch (filter.index()) {
    case 0: return gaussian(source, std::get<GaussianBlur>(filter).radius);
    case 1: return unsharp(source, std::get<UnsharpMask>(filter));
    case 2: return noise(source, std::get<AddNoise>(filter), origin);
    case 3: return motion(source, std::get<MotionBlur>(filter));
  }
  return source;
}

}  // namespace ps
