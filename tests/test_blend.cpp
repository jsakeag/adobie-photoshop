#include <algorithm>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/blend.h"
#include "ps/compositor.h"

using namespace ps;
using Catch::Approx;

namespace {

// Blends 8-bit colours the way the compositor does for one pixel.
Color over(Color dst, Color src, float opacity = 1.0f) {
  Premul s = premultiply(src);
  s.r *= opacity;
  s.g *= opacity;
  s.b *= opacity;
  s.a *= opacity;
  return unpremultiply(blend(BlendMode::Normal, premultiply(dst), s));
}

}  // namespace

TEST_CASE("Premultiply round-trips every visible 8-bit colour", "[blend]") {
  int mismatches = 0;
  for (int a = 1; a <= 255; ++a) {
    for (int v = 0; v <= 255; ++v) {
      const Color c{static_cast<std::uint8_t>(v),
                    static_cast<std::uint8_t>(255 - v),
                    static_cast<std::uint8_t>(v / 2),
                    static_cast<std::uint8_t>(a)};
      mismatches += unpremultiply(premultiply(c)) != c;
    }
  }
  CHECK(mismatches == 0);
}

TEST_CASE("Premultiply scales colour by alpha", "[blend]") {
  const Premul p = premultiply({255, 0, 51, 51});
  CHECK(p.a == Approx(0.2));
  CHECK(p.r == Approx(0.2));
  CHECK(p.g == Approx(0.0));
  CHECK(p.b == Approx(0.04));

  // Colour hidden behind zero alpha does not survive.
  CHECK(unpremultiply(premultiply({255, 128, 0, 0})) == kTransparent);
}

TEST_CASE("Normal blend", "[blend]") {
  const Color red{255, 0, 0, 255};
  const Color blue{0, 0, 255, 255};

  SECTION("an opaque source replaces the destination") {
    CHECK(over(blue, red) == red);
    CHECK(over(kTransparent, red) == red);
    CHECK(over({10, 20, 30, 40}, red) == red);
  }

  SECTION("a transparent source leaves the destination alone") {
    CHECK(over(blue, kTransparent) == blue);
    CHECK(over({10, 20, 30, 40}, kTransparent) == Color{10, 20, 30, 40});
    CHECK(over(blue, red, 0.0f) == blue);
  }

  SECTION("half opacity mixes evenly over an opaque destination") {
    CHECK(over(kWhite, kBlack, 0.5f) == Color{128, 128, 128, 255});
    CHECK(over(blue, red, 0.5f) == Color{128, 0, 128, 255});
  }

  SECTION("source alpha and opacity multiply") {
    // 50% alpha at 50% opacity covers a quarter.
    CHECK(over(kWhite, {0, 0, 0, 128}, 0.5f) == Color{191, 191, 191, 255});
  }

  SECTION("over transparency the source keeps its colour") {
    CHECK(over(kTransparent, {200, 100, 50, 128}) == Color{200, 100, 50, 128});
    CHECK(over(kTransparent, red, 0.5f) == Color{255, 0, 0, 128});
  }

  SECTION("two translucent pixels accumulate coverage") {
    // 1 - (1 - 0.5)(1 - 0.5) = 0.75 alpha; red contributes 1/3 of the colour.
    const Color half_red{255, 0, 0, 128};
    const Color half_blue{0, 0, 255, 128};
    const Color result = over(half_red, half_blue);
    CHECK(result.a == 192);
    CHECK(result.r == 85);
    CHECK(result.g == 0);
    CHECK(result.b == 170);
  }
}

TEST_CASE("Normal blend matches its golden image", "[blend][golden]") {
  // A translucent test pattern over a striped, partly transparent backdrop.
  const int size = 96;
  Layer backdrop;
  for (int i = 0; i < 6; ++i) {
    const std::uint8_t level = static_cast<std::uint8_t>(i * 51);
    backdrop.pixels.fill({i * 16, 0, 16, 72}, {level, level, 255, 255});
  }
  Layer pattern;
  pattern.pixels.write(make_test_pattern(64, 64), {0, 0});
  pattern.offset = {16, 20};
  pattern.opacity = 0.8f;

  LayerStack stack;
  stack.insert(0, backdrop);
  stack.insert(1, pattern);

  Compositor compositor;
  CHECK(golden_mismatch(compositor.render(stack, {0, 0, size, size}),
                        "blend_normal") == "");
}

namespace {

struct NamedMode {
  BlendMode mode;
  const char* name;
};

const NamedMode kModes[] = {
    {BlendMode::Multiply, "multiply"},
    {BlendMode::Screen, "screen"},
    {BlendMode::Overlay, "overlay"},
    {BlendMode::SoftLight, "soft_light"},
    {BlendMode::HardLight, "hard_light"},
    {BlendMode::Darken, "darken"},
    {BlendMode::Lighten, "lighten"},
    {BlendMode::Difference, "difference"},
    {BlendMode::ColorDodge, "color_dodge"},
    {BlendMode::ColorBurn, "color_burn"},
};

Color mix(BlendMode mode, Color dst, Color src) {
  return unpremultiply(blend(mode, premultiply(dst), premultiply(src)));
}

// One channel of an opaque grey source blended onto an opaque grey backdrop.
int grey(BlendMode mode, int backdrop, int source) {
  const auto b = static_cast<std::uint8_t>(backdrop);
  const auto s = static_cast<std::uint8_t>(source);
  const Color result = mix(mode, {b, b, b, 255}, {s, s, s, 255});
  REQUIRE(result.a == 255);
  REQUIRE(result.r == result.g);
  REQUIRE(result.g == result.b);
  return result.r;
}

}  // namespace

TEST_CASE("Blend modes give the expected values", "[blend]") {
  // 102 is 0.4 and 204 is 0.8, so the expected values are easy to derive.
  SECTION("Multiply: b * s") {
    CHECK(grey(BlendMode::Multiply, 102, 204) == 82);   // 0.32
    CHECK(grey(BlendMode::Multiply, 204, 102) == 82);
  }
  SECTION("Screen: b + s - b * s") {
    CHECK(grey(BlendMode::Screen, 102, 204) == 224);    // 0.88
    CHECK(grey(BlendMode::Screen, 204, 102) == 224);
  }
  SECTION("Overlay: multiply or screen, chosen by the backdrop") {
    CHECK(grey(BlendMode::Overlay, 102, 204) == 163);   // 2 * 0.4 * 0.8
    CHECK(grey(BlendMode::Overlay, 204, 102) == 194);   // 1 - 2 * 0.2 * 0.6
  }
  SECTION("Hard Light: multiply or screen, chosen by the source") {
    CHECK(grey(BlendMode::HardLight, 102, 204) == 194);
    CHECK(grey(BlendMode::HardLight, 204, 102) == 163);
  }
  SECTION("Soft Light") {
    CHECK(grey(BlendMode::SoftLight, 102, 204) == 138);  // 0.16 + 0.6 * sqrt(0.4)
    CHECK(grey(BlendMode::SoftLight, 204, 102) == 196);  // 0.64 + 0.64 * 0.2
    CHECK(grey(BlendMode::SoftLight, 64, 255) == 128);   // sqrt(b)
    CHECK(grey(BlendMode::SoftLight, 128, 0) == 64);     // b * b
  }
  SECTION("Darken and Lighten: min and max") {
    CHECK(grey(BlendMode::Darken, 102, 204) == 102);
    CHECK(grey(BlendMode::Darken, 204, 102) == 102);
    CHECK(grey(BlendMode::Lighten, 102, 204) == 204);
    CHECK(grey(BlendMode::Lighten, 204, 102) == 204);
  }
  SECTION("Difference: |b - s|") {
    CHECK(grey(BlendMode::Difference, 102, 204) == 102);
    CHECK(grey(BlendMode::Difference, 204, 102) == 102);
    CHECK(grey(BlendMode::Difference, 77, 77) == 0);
  }
  SECTION("Color Dodge: b / (1 - s)") {
    CHECK(grey(BlendMode::ColorDodge, 51, 102) == 85);   // 0.2 / 0.6
    CHECK(grey(BlendMode::ColorDodge, 102, 204) == 255); // clips
    CHECK(grey(BlendMode::ColorDodge, 0, 255) == 0);     // black stays black
    CHECK(grey(BlendMode::ColorDodge, 1, 255) == 255);
  }
  SECTION("Color Burn: 1 - (1 - b) / s") {
    CHECK(grey(BlendMode::ColorBurn, 102, 204) == 64);   // 1 - 0.6 / 0.8
    CHECK(grey(BlendMode::ColorBurn, 204, 153) == 170);  // 1 - 0.2 / 0.6
    CHECK(grey(BlendMode::ColorBurn, 51, 102) == 0);     // clips
    CHECK(grey(BlendMode::ColorBurn, 255, 0) == 255);    // white stays white
    CHECK(grey(BlendMode::ColorBurn, 254, 0) == 0);
  }
}

TEST_CASE("Blend modes have their neutral and extreme colours", "[blend]") {
  int wrong = 0;
  for (int b = 0; b <= 255; ++b) {
    // Neutral: the backdrop comes through unchanged.
    wrong += grey(BlendMode::Multiply, b, 255) != b;
    wrong += grey(BlendMode::Screen, b, 0) != b;
    wrong += grey(BlendMode::Darken, b, 255) != b;
    wrong += grey(BlendMode::Lighten, b, 0) != b;
    wrong += grey(BlendMode::Difference, b, 0) != b;
    wrong += grey(BlendMode::ColorDodge, b, 0) != b;
    wrong += grey(BlendMode::ColorBurn, b, 255) != b;
    // Extreme.
    wrong += grey(BlendMode::Multiply, b, 0) != 0;
    wrong += grey(BlendMode::Screen, b, 255) != 255;
    wrong += grey(BlendMode::Difference, b, 255) != 255 - b;
    wrong += grey(BlendMode::HardLight, b, 0) != 0;
    wrong += grey(BlendMode::HardLight, b, 255) != 255;
    wrong += grey(BlendMode::Overlay, 0, b) != 0;
    wrong += grey(BlendMode::Overlay, 255, b) != 255;
    wrong += grey(BlendMode::SoftLight, 0, b) != 0;
    wrong += grey(BlendMode::SoftLight, 255, b) != 255;
  }
  CHECK(wrong == 0);
}

TEST_CASE("Blend modes work per channel", "[blend]") {
  const Color backdrop{204, 102, 0, 255};
  const Color source{102, 204, 255, 255};
  CHECK(mix(BlendMode::Multiply, backdrop, source) == Color{82, 82, 0, 255});
  CHECK(mix(BlendMode::Screen, backdrop, source) == Color{224, 224, 255, 255});
  CHECK(mix(BlendMode::Darken, backdrop, source) == Color{102, 102, 0, 255});
  CHECK(mix(BlendMode::Lighten, backdrop, source) ==
        Color{204, 204, 255, 255});
  CHECK(mix(BlendMode::Difference, backdrop, source) ==
        Color{102, 102, 255, 255});
}

TEST_CASE("Blend modes only act where both pixels have coverage", "[blend]") {
  const Color white_half{255, 255, 255, 128};
  const Color red{255, 0, 0, 255};

  for (const NamedMode& m : kModes) {
    CAPTURE(m.name);
    // Nothing underneath: the source shows as it is.
    CHECK(mix(m.mode, kTransparent, {200, 100, 50, 128}) ==
          Color{200, 100, 50, 128});
    CHECK(mix(m.mode, kTransparent, red) == red);
    // Nothing on top: the backdrop is untouched.
    CHECK(mix(m.mode, {10, 200, 30, 90}, kTransparent) ==
          Color{10, 200, 30, 90});
    // Alpha accumulates the same way in every mode.
    CHECK(mix(m.mode, white_half, {0, 0, 0, 128}).a == 192);
    CHECK(mix(m.mode, white_half, red).a == 255);
  }

  SECTION("a translucent source blends only its covered share") {
    // Half-covering black multiplies half of white to black: 127 of 255.
    CHECK(mix(BlendMode::Multiply, kWhite, {0, 0, 0, 128}) ==
          Color{127, 127, 127, 255});
  }

  SECTION("a translucent backdrop lets the unblended source through") {
    // Multiply by white is neutral where the backdrop exists, and the source
    // shows directly where it does not: plain red either way.
    CHECK(mix(BlendMode::Multiply, white_half, red) == red);
    // Black over the same backdrop is black both ways.
    CHECK(mix(BlendMode::Multiply, white_half, kBlack) == kBlack);
    // Difference with white inverts red to cyan in the covered half only.
    CHECK(mix(BlendMode::Difference, white_half, red) ==
          Color{127, 128, 128, 255});
  }
}

TEST_CASE("Blend modes match their golden images", "[blend][golden]") {
  // Backdrop varies left to right, source top to bottom, so each quadrant
  // is a lookup table: grey on grey, colour on grey, grey on colour and
  // colour on colour. The right strip has no backdrop and the bottom band
  // has a half-transparent source.
  const int size = 128;
  Image backdrop_pixels(size, size);
  Image source_pixels(size, size);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const auto bx = static_cast<std::uint8_t>(std::min(x, 111) * 255 / 111);
      const auto sy = static_cast<std::uint8_t>(y * 255 / (size - 1));
      const auto inv_bx = static_cast<std::uint8_t>(255 - bx);
      const auto inv_sy = static_cast<std::uint8_t>(255 - sy);
      if (x < 112) {
        backdrop_pixels.set_pixel(
            x, y, y < 64 ? Color{bx, bx, bx, 255} : Color{bx, inv_bx, 128, 255});
      }
      const std::uint8_t alpha = y < 96 ? 255 : 128;
      source_pixels.set_pixel(
          x, y, x < 56 ? Color{sy, sy, sy, alpha} : Color{inv_sy, sy, 64, alpha});
    }
  }

  Layer backdrop;
  backdrop.pixels.write(backdrop_pixels, {0, 0});
  Layer source;
  source.pixels.write(source_pixels, {0, 0});

  for (const NamedMode& m : kModes) {
    source.blend = m.mode;
    LayerStack stack;
    stack.insert(0, backdrop);
    stack.insert(1, source);
    Compositor compositor;
    CHECK(golden_mismatch(compositor.render(stack, {0, 0, size, size}),
                          std::string("blend_") + m.name) == "");
  }
}
