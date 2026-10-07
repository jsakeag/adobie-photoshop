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
