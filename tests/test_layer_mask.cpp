#include <cstdint>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/compositor.h"
#include "ps/document.h"

using namespace ps;

namespace {

constexpr int kT = kTileSize;
const Color kRed{255, 0, 0, 255};

Layer solid(Rect area, Color c) {
  Layer layer;
  layer.pixels.fill(area, c);
  return layer;
}

Image composite(const LayerStack& stack, Rect r) {
  Compositor compositor;
  return compositor.render(stack, r);
}

// What the document should show, composited from scratch with no cache.
Image uncached(const Document& doc) {
  return composite(doc.layers(), doc.bounds());
}

// A white document with a red 40x20 layer at (10, 10), masked to show only
// its left half.
Document half_masked() {
  Document doc(2 * kT, kT);
  Layer red = solid({0, 0, 40, 20}, kRed);
  red.offset = {10, 10};
  red.mask = MaskGrid(255);
  red.mask->fill({20, 0, 20, 20}, 0);
  doc.add_layer(1, red);
  return doc;
}

}  // namespace

TEST_CASE("A layer mask scales the layer's coverage", "[mask][compositor]") {
  LayerStack stack;
  stack.insert(0, solid({0, 0, 20, 20}, kWhite));
  stack.insert(1, solid({0, 0, 20, 20}, kBlack));
  const Rect area{0, 0, 20, 20};

  SECTION("a reveal-all mask changes nothing") {
    stack.at(1).mask = MaskGrid(255);
    CHECK(composite(stack, area) == Image(20, 20, kBlack));
  }

  SECTION("a hide-all mask removes the layer") {
    stack.at(1).mask = MaskGrid(0);
    CHECK(composite(stack, area) == Image(20, 20, kWhite));
  }

  SECTION("painting on the mask shows and hides parts") {
    stack.at(1).mask = MaskGrid(0);
    stack.at(1).mask->fill({5, 5, 10, 10}, 255);
    const Image result = composite(stack, area);
    CHECK(result.pixel(4, 5) == kWhite);
    CHECK(result.pixel(5, 5) == kBlack);
    CHECK(result.pixel(14, 14) == kBlack);
    CHECK(result.pixel(15, 14) == kWhite);
  }

  SECTION("grey mask values give partial coverage") {
    stack.at(1).mask = MaskGrid(128);
    CHECK(composite(stack, area) == Image(20, 20, {127, 127, 127, 255}));
  }

  SECTION("mask and opacity multiply") {
    stack.at(1).mask = MaskGrid(128);
    stack.at(1).opacity = 0.5f;
    CHECK(composite(stack, area) == Image(20, 20, {191, 191, 191, 255}));
  }

  SECTION("alone, a masked layer leaves transparency") {
    stack.remove(0);
    stack.at(0).mask = MaskGrid(255);
    stack.at(0).mask->fill({0, 0, 10, 20}, 0);
    const Image result = composite(stack, area);
    CHECK(result.pixel(9, 0) == kTransparent);
    CHECK(result.pixel(10, 0) == kBlack);
  }

  SECTION("the mask applies before the blend mode") {
    stack.at(1).blend = BlendMode::Multiply;
    stack.at(0).pixels.fill(area, kRed);
    stack.at(1).mask = MaskGrid(255);
    stack.at(1).mask->fill({0, 0, 10, 20}, 0);
    const Image result = composite(stack, area);
    CHECK(result.pixel(9, 0) == kRed);     // masked out: backdrop untouched
    CHECK(result.pixel(10, 0) == kBlack);  // red multiplied by black
  }
}

TEST_CASE("A layer mask moves with its layer", "[mask][compositor]") {
  Layer layer = solid({0, 0, 20, 20}, kRed);
  layer.mask = MaskGrid(255);
  layer.mask->fill({0, 0, 10, 20}, 0);  // hide the layer's left half
  layer.offset = {kT - 5, 30};          // straddles a tile boundary
  LayerStack stack;
  stack.insert(0, layer);

  const Image result = composite(stack, {0, 0, 2 * kT, kT});
  CHECK(result.pixel(kT - 5, 30) == kTransparent);
  CHECK(result.pixel(kT + 4, 30) == kTransparent);
  CHECK(result.pixel(kT + 5, 30) == kRed);
  CHECK(result.pixel(kT + 14, 49) == kRed);
  CHECK(result.pixel(kT + 15, 49) == kTransparent);
}

TEST_CASE("Document layer blend mode", "[document][layers]") {
  Document doc(2 * kT, 2 * kT, {204, 204, 204, 255});
  doc.add_layer(1, solid({0, 0, 10, 10}, {102, 102, 102, 255}));
  CHECK(doc.flatten().pixel(5, 5) == Color{102, 102, 102, 255});
  const std::size_t before = doc.compositor().tiles_composited();

  doc.set_layer_blend(1, BlendMode::Multiply);
  CHECK(doc.layers().at(1).blend == BlendMode::Multiply);
  CHECK(doc.flatten().pixel(5, 5) == Color{82, 82, 82, 255});
  CHECK(doc.flatten().pixel(15, 15) == Color{204, 204, 204, 255});
  CHECK(doc.compositor().tiles_composited() == before + 1);

  doc.set_layer_blend(1, BlendMode::Screen);
  CHECK(doc.flatten().pixel(5, 5) == Color{224, 224, 224, 255});

  CHECK(doc.flatten() == uncached(doc));
  CHECK_THROWS_AS(doc.set_layer_blend(2, BlendMode::Normal),
                  std::out_of_range);
}

TEST_CASE("Document layer mask edits show up in the composite",
          "[document][layers][mask]") {
  Document doc = half_masked();
  CHECK(doc.flatten().pixel(10, 10) == kRed);
  CHECK(doc.flatten().pixel(29, 29) == kRed);
  CHECK(doc.flatten().pixel(30, 10) == kWhite);  // hidden half
  CHECK(doc.flatten().pixel(49, 29) == kWhite);

  SECTION("remove the mask") {
    doc.set_layer_mask(1, std::nullopt);
    CHECK_FALSE(doc.layers().at(1).mask.has_value());
    CHECK(doc.flatten().pixel(49, 29) == kRed);
  }

  SECTION("replace the mask") {
    doc.set_layer_mask(1, MaskGrid(0));
    CHECK(doc.flatten().pixel(10, 10) == kWhite);
  }

  SECTION("paint on the mask, in layer coordinates") {
    doc.edit_layer_mask(1, {30, 0, 5, 20}).fill({30, 0, 5, 20}, 255);
    CHECK(doc.flatten().pixel(39, 10) == kWhite);
    CHECK(doc.flatten().pixel(40, 10) == kRed);
    CHECK(doc.flatten().pixel(44, 29) == kRed);
    CHECK(doc.flatten().pixel(45, 29) == kWhite);
  }

  SECTION("moving the layer carries the mask") {
    doc.set_layer_offset(1, {kT + 10, 10});
    CHECK(doc.flatten().pixel(10, 10) == kWhite);
    CHECK(doc.flatten().pixel(kT + 29, 29) == kRed);
    CHECK(doc.flatten().pixel(kT + 30, 10) == kWhite);
  }

  CHECK(doc.flatten() == uncached(doc));
}

TEST_CASE("Editing a missing mask is an error", "[document][layers][mask]") {
  Document doc(20, 20);
  CHECK_THROWS_AS(doc.edit_layer_mask(0, {0, 0, 5, 5}), std::logic_error);
  CHECK_THROWS_AS(doc.edit_layer_mask(3, {0, 0, 5, 5}), std::out_of_range);
  CHECK_THROWS_AS(doc.set_layer_mask(3, MaskGrid(0)), std::out_of_range);
}

TEST_CASE("A mask edit recomposites only the tiles it touches",
          "[document][layers][mask]") {
  Document doc(4 * kT, 2 * kT);
  Layer red = solid({0, 0, 4 * kT, 2 * kT}, kRed);
  red.mask = MaskGrid(255);
  doc.add_layer(1, red);
  doc.flatten();
  const std::size_t before = doc.compositor().tiles_composited();
  CHECK(before == 8);

  doc.edit_layer_mask(1, {kT + 5, 5, 4, 4}).fill({kT + 5, 5, 4, 4}, 0);
  CHECK(doc.flatten().pixel(kT + 6, 6) == kWhite);
  CHECK(doc.compositor().tiles_composited() == before + 1);
  CHECK(doc.flatten() == uncached(doc));
}

TEST_CASE("Canvas operations carry layer masks along",
          "[document][layers][mask]") {
  Document doc = half_masked();  // red shows at x 10..29, y 10..29
  doc.flatten();
  const int w = doc.width(), h = doc.height();

  SECTION("flip horizontally") {
    doc.flip_horizontal();
    CHECK(doc.flatten().pixel(w - 11, 10) == kRed);
    CHECK(doc.flatten().pixel(w - 30, 29) == kRed);
    CHECK(doc.flatten().pixel(w - 31, 29) == kWhite);  // the hidden half
    CHECK(doc.flatten().pixel(w - 50, 10) == kWhite);
  }

  SECTION("flip vertically") {
    doc.flip_vertical();
    CHECK(doc.flatten().pixel(10, h - 11) == kRed);
    CHECK(doc.flatten().pixel(29, h - 30) == kRed);
    CHECK(doc.flatten().pixel(30, h - 30) == kWhite);
  }

  SECTION("rotate clockwise") {
    doc.rotate(Rotation::CW90);
    // (x, y) lands at (h - 1 - y, x).
    CHECK(doc.flatten().pixel(h - 11, 10) == kRed);
    CHECK(doc.flatten().pixel(h - 30, 29) == kRed);
    CHECK(doc.flatten().pixel(h - 30, 30) == kWhite);
    CHECK(doc.flatten().pixel(h - 11, 49) == kWhite);
  }

  SECTION("rotating all the way round restores the picture") {
    const Image original = doc.flatten();
    for (int i = 0; i < 4; ++i) doc.rotate(Rotation::CCW90);
    CHECK(doc.flatten() == original);
    doc.rotate(Rotation::R180);
    doc.rotate(Rotation::R180);
    CHECK(doc.flatten() == original);
  }

  SECTION("crop and canvas resize") {
    doc.crop({20, 5, 40, 30});
    CHECK(doc.flatten().pixel(9, 5) == kRed);
    CHECK(doc.flatten().pixel(10, 5) == kWhite);
    doc.resize_canvas(60, 40, Anchor::BottomRight);
    CHECK(doc.flatten().pixel(29, 15) == kRed);
    CHECK(doc.flatten().pixel(30, 15) == kWhite);
  }

  REQUIRE(doc.layers().at(1).mask.has_value());
  CHECK(doc.flatten() == uncached(doc));
}

TEST_CASE("A gradient mask matches its golden image", "[mask][golden]") {
  // The test pattern fading out left to right over a white background.
  const int size = 96;
  Document doc(size, size);
  Layer pattern;
  pattern.pixels.write(make_test_pattern(80, 80), {0, 0});
  pattern.offset = {8, 8};
  pattern.mask = MaskGrid(0);
  std::vector<std::uint8_t> ramp;
  for (int y = 0; y < 80; ++y) {
    for (int x = 0; x < 80; ++x) {
      ramp.push_back(static_cast<std::uint8_t>(255 - x * 255 / 79));
    }
  }
  pattern.mask->write(ramp, {0, 0, 80, 80});
  doc.add_layer(1, pattern);
  CHECK(golden_mismatch(doc.flatten(), "mask_gradient") == "");
}
