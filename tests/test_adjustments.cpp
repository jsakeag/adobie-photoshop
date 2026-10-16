#include <cmath>
#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/adjust_commands.h"
#include "ps/selection.h"
#include "state_checks.h"

using namespace ps;

namespace {

const Color kRed{255, 0, 0, 255};

bool is_identity(const Lut& lut) {
  for (int v = 0; v < 256; ++v) {
    if (lut[v] != v) return false;
  }
  return true;
}

// Hue across, lightness down; the bottom band is a grey ramp.
Image chart() {
  const int w = 120, h = 80;
  Image image(w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (y >= 64) {
        const auto v = static_cast<std::uint8_t>(x * 255 / (w - 1));
        image.set_pixel(x, y, {v, v, v, 255});
        continue;
      }
      // Rotate pure red to the column's hue, then set the row's lightness.
      HueSaturation hs;
      hs.hue = x * 360 / w - 180;
      hs.lightness = (y - 32) * 3;
      const Color base = adjust_hue_saturation({0, 255, 255, 255}, {0, 0, 0});
      image.set_pixel(x, y, adjust_hue_saturation(base, hs));
    }
  }
  return image;
}

}  // namespace

TEST_CASE("Default adjustments change nothing", "[adjust]") {
  CHECK(is_identity(levels_lut({})));
  CHECK(is_identity(curves_lut({})));
  CHECK(is_identity(brightness_contrast_lut({})));
  int wrong = 0;
  for (int r = 0; r < 256; r += 15) {
    for (int g = 0; g < 256; g += 15) {
      for (int b = 0; b < 256; b += 15) {
        const Color c{std::uint8_t(r), std::uint8_t(g), std::uint8_t(b), 200};
        wrong += adjust_hue_saturation(c, {}) != c;
      }
    }
  }
  CHECK(wrong == 0);
}

TEST_CASE("Levels", "[adjust]") {
  Levels black_point;
  black_point.input_black = 50;
  Lut lut = levels_lut(black_point);
  CHECK(lut[0] == 0);
  CHECK(lut[50] == 0);
  CHECK(lut[152] == 127);  // 102 / 205 of the way
  CHECK(lut[255] == 255);

  Levels white_point;
  white_point.input_white = 200;
  lut = levels_lut(white_point);
  CHECK(lut[50] == 64);  // 63.75
  CHECK(lut[200] == 255);
  CHECK(lut[230] == 255);

  Levels gamma;
  gamma.gamma = 2.0f;
  lut = levels_lut(gamma);
  CHECK(lut[64] == 128);  // sqrt(0.251) of the way
  CHECK(lut[0] == 0);
  CHECK(lut[255] == 255);

  Levels output;
  output.output_black = 100;
  output.output_white = 200;
  lut = levels_lut(output);
  CHECK(lut[0] == 100);
  CHECK(lut[51] == 120);
  CHECK(lut[255] == 200);
}

TEST_CASE("Curves", "[adjust]") {
  CHECK(curves_lut({{{0, 255}, {255, 0}}}) == invert_lut());

  Curves s_curve{{{0, 0}, {64, 40}, {128, 128}, {192, 215}, {255, 255}}};
  const Lut lut = curves_lut(s_curve);
  CHECK(lut[0] == 0);
  CHECK(lut[64] == 40);  // passes through every point
  CHECK(lut[128] == 128);
  CHECK(lut[192] == 215);
  CHECK(lut[255] == 255);
  for (int v = 0; v < 255; ++v) {
    CAPTURE(v);
    CHECK(lut[v] <= lut[v + 1]);  // rising points give a rising curve
  }

  SECTION("no overshoot near a plateau") {
    const Lut flat = curves_lut({{{0, 0}, {100, 200}, {150, 200}, {255, 255}}});
    for (int v = 100; v <= 150; ++v) {
      CAPTURE(v);
      CHECK(flat[v] == 200);
    }
  }
  SECTION("flat beyond the first and last points") {
    const Lut inner = curves_lut({{{50, 20}, {200, 220}}});
    CHECK(inner[0] == 20);
    CHECK(inner[50] == 20);
    CHECK(inner[200] == 220);
    CHECK(inner[255] == 220);
  }
  SECTION("order and duplicates do not matter") {
    CHECK(curves_lut({{{255, 255}, {128, 100}, {0, 0}}}) ==
          curves_lut({{{0, 0}, {128, 50}, {128, 100}, {255, 255}}}));
  }
  SECTION("fewer than two points is the identity") {
    CHECK(is_identity(curves_lut({{{100, 30}}})));
  }
}

TEST_CASE("Brightness and contrast", "[adjust]") {
  Lut lut = brightness_contrast_lut({50, 0});
  CHECK(lut[0] == 50);
  CHECK(lut[100] == 150);
  CHECK(lut[230] == 255);

  lut = brightness_contrast_lut({-50, 0});
  CHECK(lut[30] == 0);
  CHECK(lut[100] == 50);

  lut = brightness_contrast_lut({0, 25});  // stretch by 4/3
  CHECK(lut[200] == 224);
  CHECK(lut[55] == 31);

  lut = brightness_contrast_lut({0, -50});  // squeeze by half
  CHECK(lut[200] == 164);

  lut = brightness_contrast_lut({0, -100});
  for (int v = 0; v < 256; ++v) CHECK(lut[v] == 128);

  lut = brightness_contrast_lut({0, 100});
  CHECK(lut[127] == 0);
  CHECK(lut[128] == 255);
}

TEST_CASE("Hue, saturation and lightness", "[adjust]") {
  CHECK(adjust_hue_saturation(kRed, {120, 0, 0}) == Color{0, 255, 0, 255});
  CHECK(adjust_hue_saturation(kRed, {180, 0, 0}) == Color{0, 255, 255, 255});
  CHECK(adjust_hue_saturation(kRed, {-120, 0, 0}) == Color{0, 0, 255, 255});
  CHECK(adjust_hue_saturation(kRed, {0, -100, 0}) == Color{128, 128, 128, 255});
  CHECK(adjust_hue_saturation(kRed, {0, 0, 100}) == kWhite);
  CHECK(adjust_hue_saturation(kRed, {0, 0, -100}) == kBlack);
  CHECK(adjust_hue_saturation({10, 20, 30, 7}, {50, 50, 50}).a == 7);

  // Greys have no hue to rotate or saturation to scale.
  const Color grey{90, 90, 90, 255};
  CHECK(adjust_hue_saturation(grey, {77, 100, 0}) == grey);

  // More saturation pulls the channels apart.
  const Color dull{150, 100, 100, 255};
  const Color vivid = adjust_hue_saturation(dull, {0, 60, 0});
  CHECK(vivid.r - vivid.g > dull.r - dull.g);
  CHECK(vivid.g == vivid.b);
}

TEST_CASE("Invert", "[adjust]") {
  const auto invert = adjustment_function(Invert{});
  CHECK(invert({0, 100, 255, 80}) == Color{255, 155, 0, 80});
}

TEST_CASE("Adjustments have names", "[adjust]") {
  CHECK(std::string(adjustment_name(Levels{})) == "Levels");
  CHECK(std::string(adjustment_name(Curves{})) == "Curves");
  CHECK(std::string(adjustment_name(BrightnessContrast{})) == "Brightness/Contrast");
  CHECK(std::string(adjustment_name(HueSaturation{})) == "Hue/Saturation");
  CHECK(std::string(adjustment_name(Invert{})) == "Invert");
  CHECK(AdjustCommand(0, Invert{}).name() == "Invert");
}

TEST_CASE("AdjustCommand changes one layer within the selection", "[adjust]") {
  Document doc(Image(40, 40, {200, 100, 50, 255}));
  Layer top;
  top.pixels.fill({0, 0, 10, 10}, kRed);
  top.offset = {30, 30};
  doc.add_layer(1, top);

  SECTION("the whole layer") {
    AdjustCommand(0, Invert{}).execute(doc);
    CHECK(doc.layers().at(0).pixels.pixel(5, 5) == Color{55, 155, 205, 255});
    CHECK(doc.layers().at(1).pixels.pixel(5, 5) == kRed);  // other layers alone
  }
  SECTION("a moved layer") {
    AdjustCommand(1, Invert{}).execute(doc);
    CHECK(doc.layers().at(1).pixels.pixel(5, 5) == Color{0, 255, 255, 255});
    CHECK(doc.flatten().pixel(35, 35) == Color{0, 255, 255, 255});
  }
  SECTION("only the selection") {
    doc.set_selection(rect_mask({0, 0, 20, 40}));
    AdjustCommand(0, Invert{}).execute(doc);
    CHECK(doc.layers().at(0).pixels.pixel(19, 5) == Color{55, 155, 205, 255});
    CHECK(doc.layers().at(0).pixels.pixel(20, 5) == Color{200, 100, 50, 255});
  }
  SECTION("a partly selected area blends") {
    MaskGrid half(0);
    half.fill({0, 0, 40, 40}, 128);
    doc.set_selection(half);
    AdjustCommand(0, Levels{0, 255, 1.0f, 255, 255}).execute(doc);  // all white
    CHECK(doc.layers().at(0).pixels.pixel(5, 5) == Color{228, 178, 153, 255});
  }
  SECTION("transparent pixels and pixels off the canvas stay as they are") {
    Layer wide;
    wide.pixels.fill({-10, 0, 20, 5}, {10, 10, 10, 255});
    wide.pixels.set_pixel(5, 10, {10, 20, 30, 0});
    doc.add_layer(2, wide);
    AdjustCommand(2, Invert{}).execute(doc);
    CHECK(doc.layers().at(2).pixels.pixel(0, 0) == Color{245, 245, 245, 255});
    CHECK(doc.layers().at(2).pixels.pixel(-1, 0) == Color{10, 10, 10, 255});
    CHECK(doc.layers().at(2).pixels.pixel(5, 10) == Color{10, 20, 30, 0});
  }
  CHECK(cache_is_current(doc));
}

TEST_CASE("Adjustments match their golden images", "[adjust][golden]") {
  Curves s_curve{{{0, 0}, {64, 30}, {128, 128}, {192, 225}, {255, 255}}};
  const struct {
    const char* name;
    Adjustment adjustment;
  } cases[] = {
      {"adjust_levels", Levels{40, 210, 1.6f, 20, 240}},
      {"adjust_curves", s_curve},
      {"adjust_brightness_contrast", BrightnessContrast{20, 40}},
      {"adjust_hue_saturation", HueSaturation{60, -40, 15}},
      {"adjust_invert", Invert{}},
  };
  for (const auto& c : cases) {
    CAPTURE(c.name);
    Document doc(chart());
    AdjustCommand(0, c.adjustment).execute(doc);
    CHECK(golden_mismatch(doc.flatten(), c.name) == "");
  }
  // The source chart itself, so the others can be compared with it.
  CHECK(golden_mismatch(chart(), "adjust_source") == "");
}
