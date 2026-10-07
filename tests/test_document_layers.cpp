#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/document.h"

using namespace ps;

namespace {

constexpr int kT = kTileSize;
const Color kRed{255, 0, 0, 255};
const Color kBlue{0, 0, 255, 255};

Layer solid(const char* name, Rect area, Color c) {
  Layer layer;
  layer.name = name;
  layer.pixels.fill(area, c);
  return layer;
}

// What the document should show, composited from scratch with no cache.
Image uncached(const Document& doc) {
  Compositor fresh;
  return fresh.render(doc.layers(), doc.bounds());
}

// A 4x3-tile white document with a small red layer in the top-left tile.
Document sample() {
  Document doc(4 * kT, 3 * kT);
  doc.add_layer(1, solid("red", {8, 8, 16, 16}, kRed));
  return doc;
}

}  // namespace

TEST_CASE("A new document has one background layer", "[document][layers]") {
  const Document blank(20, 10);
  REQUIRE(blank.layers().size() == 1);
  CHECK(blank.layers().at(0).name == "Background");
  CHECK(blank.flatten() == Image(20, 10, kWhite));

  const Image pattern = make_test_pattern(30, 20);
  const Document opened(pattern);
  REQUIRE(opened.layers().size() == 1);
  CHECK(opened.layers().at(0).pixels.read({0, 0, 30, 20}) == pattern);
}

TEST_CASE("Document layer edits show up in the composite",
          "[document][layers]") {
  Document doc = sample();
  doc.flatten();  // fill the cache so stale tiles would be noticed

  SECTION("add on top") {
    doc.add_layer(2, solid("blue", {16, 16, 16, 16}, kBlue));
    CHECK(doc.layers().size() == 3);
    CHECK(doc.flatten().pixel(10, 10) == kRed);
    CHECK(doc.flatten().pixel(20, 20) == kBlue);
    CHECK(doc.flatten().pixel(40, 40) == kWhite);
  }

  SECTION("add below an existing layer") {
    doc.add_layer(1, solid("blue", {16, 16, 16, 16}, kBlue));
    CHECK(doc.layers().at(1).name == "blue");
    CHECK(doc.flatten().pixel(20, 20) == kRed);  // red is still on top
    CHECK(doc.flatten().pixel(28, 28) == kBlue);
  }

  SECTION("an added layer's opacity is clamped") {
    Layer layer = solid("blue", {0, 0, 8, 8}, kBlue);
    layer.opacity = 7.0f;
    doc.add_layer(2, layer);
    CHECK(doc.layers().at(2).opacity == 1.0f);
  }

  SECTION("remove") {
    const Layer removed = doc.remove_layer(1);
    CHECK(removed.name == "red");
    CHECK(doc.layers().size() == 1);
    CHECK(doc.flatten() == Image(4 * kT, 3 * kT, kWhite));
  }

  SECTION("reorder") {
    doc.add_layer(2, solid("blue", {16, 16, 16, 16}, kBlue));
    CHECK(doc.flatten().pixel(20, 20) == kBlue);
    doc.move_layer(2, 1);
    CHECK(doc.layers().at(1).name == "blue");
    CHECK(doc.flatten().pixel(20, 20) == kRed);
    doc.move_layer(1, 2);
    CHECK(doc.flatten().pixel(20, 20) == kBlue);
  }

  SECTION("opacity") {
    doc.set_layer_opacity(1, 0.5f);
    CHECK(doc.flatten().pixel(10, 10) == Color{255, 128, 128, 255});
    doc.set_layer_opacity(1, -3.0f);
    CHECK(doc.layers().at(1).opacity == 0.0f);
    CHECK(doc.flatten().pixel(10, 10) == kWhite);
    doc.set_layer_opacity(1, 2.0f);
    CHECK(doc.layers().at(1).opacity == 1.0f);
    CHECK(doc.flatten().pixel(10, 10) == kRed);
  }

  SECTION("visibility") {
    doc.set_layer_visible(1, false);
    CHECK(doc.flatten().pixel(10, 10) == kWhite);
    doc.set_layer_visible(1, true);
    CHECK(doc.flatten().pixel(10, 10) == kRed);
  }

  SECTION("offset clears the old position and draws the new one") {
    doc.set_layer_offset(1, {2 * kT, kT});
    CHECK(doc.flatten().pixel(10, 10) == kWhite);
    CHECK(doc.flatten().pixel(2 * kT + 10, kT + 10) == kRed);
  }

  SECTION("pixel edits") {
    doc.edit_layer_pixels(1, {12, 12, 4, 4}).fill({12, 12, 4, 4}, kBlue);
    CHECK(doc.flatten().pixel(10, 10) == kRed);
    CHECK(doc.flatten().pixel(13, 13) == kBlue);
  }

  SECTION("pixel edits on a moved layer use layer coordinates") {
    doc.set_layer_offset(1, {2 * kT, kT});
    doc.flatten();
    doc.edit_layer_pixels(1, {12, 12, 4, 4}).fill({12, 12, 4, 4}, kBlue);
    CHECK(doc.flatten().pixel(2 * kT + 13, kT + 13) == kBlue);
  }

  CHECK(doc.flatten() == uncached(doc));
}

TEST_CASE("Document layer indices are checked", "[document][layers]") {
  Document doc = sample();
  CHECK_THROWS_AS(doc.add_layer(3, Layer{}), std::out_of_range);
  CHECK_THROWS_AS(doc.remove_layer(2), std::out_of_range);
  CHECK_THROWS_AS(doc.move_layer(0, 2), std::out_of_range);
  CHECK_THROWS_AS(doc.set_layer_opacity(2, 0.5f), std::out_of_range);
  CHECK_THROWS_AS(doc.set_layer_visible(2, false), std::out_of_range);
  CHECK_THROWS_AS(doc.set_layer_offset(2, {1, 1}), std::out_of_range);
  CHECK_THROWS_AS(doc.edit_layer_pixels(2, {0, 0, 1, 1}), std::out_of_range);
  CHECK(doc.layers().size() == 2);
}

TEST_CASE("A layer edit recomposites only the tiles it touches",
          "[document][layers]") {
  Document doc = sample();  // 12 tiles; the red layer sits in tile (0, 0)
  doc.flatten();
  const std::size_t before = doc.compositor().tiles_composited();
  CHECK(before == 12);

  doc.flatten();
  CHECK(doc.compositor().tiles_composited() == before);

  SECTION("property change on a one-tile layer") {
    doc.set_layer_opacity(1, 0.25f);
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == before + 1);
  }

  SECTION("moving a layer dirties where it was and where it went") {
    doc.set_layer_offset(1, {3 * kT, 2 * kT});
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == before + 2);
  }

  SECTION("a small pixel edit on the background") {
    doc.edit_layer_pixels(0, {kT + 4, kT + 4, 2, 2})
        .fill({kT + 4, kT + 4, 2, 2}, kBlue);
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == before + 1);
  }

  SECTION("changing the background's opacity dirties the whole canvas") {
    doc.set_layer_opacity(0, 0.5f);
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == before + 12);
  }

  CHECK(doc.flatten() == uncached(doc));
}

TEST_CASE("Canvas operations carry every layer along", "[document][layers]") {
  Document doc(40, 20);
  doc.add_layer(1, solid("red", {0, 0, 10, 10}, kRed));  // top-left corner
  doc.flatten();

  SECTION("rotate clockwise") {
    doc.rotate(Rotation::CW90);
    CHECK(doc.width() == 20);
    CHECK(doc.height() == 40);
    CHECK(doc.flatten().pixel(19, 0) == kRed);  // now the top-right corner
    CHECK(doc.flatten().pixel(10, 9) == kRed);
    CHECK(doc.flatten().pixel(9, 9) == kWhite);
    CHECK(doc.flatten().pixel(19, 10) == kWhite);
  }

  SECTION("rotate counter-clockwise") {
    doc.rotate(Rotation::CCW90);
    CHECK(doc.flatten().pixel(0, 39) == kRed);  // now the bottom-left corner
    CHECK(doc.flatten().pixel(9, 30) == kRed);
    CHECK(doc.flatten().pixel(10, 30) == kWhite);
    CHECK(doc.flatten().pixel(9, 29) == kWhite);
  }

  SECTION("flip horizontally") {
    doc.flip_horizontal();
    CHECK(doc.flatten().pixel(39, 0) == kRed);
    CHECK(doc.flatten().pixel(30, 9) == kRed);
    CHECK(doc.flatten().pixel(29, 9) == kWhite);
    CHECK(doc.flatten().pixel(0, 0) == kWhite);
  }

  SECTION("flip vertically") {
    doc.flip_vertical();
    CHECK(doc.flatten().pixel(0, 19) == kRed);
    CHECK(doc.flatten().pixel(9, 10) == kRed);
    CHECK(doc.flatten().pixel(9, 9) == kWhite);
  }

  SECTION("crop") {
    doc.crop({5, 5, 20, 10});
    CHECK(doc.flatten().pixel(0, 0) == kRed);
    CHECK(doc.flatten().pixel(4, 4) == kRed);
    CHECK(doc.flatten().pixel(5, 4) == kWhite);
    CHECK(doc.flatten().pixel(4, 5) == kWhite);
  }

  SECTION("grow the canvas") {
    doc.resize_canvas(60, 30, Anchor::BottomRight, kBlue);
    CHECK(doc.flatten().pixel(0, 0) == kBlue);    // new area
    CHECK(doc.flatten().pixel(20, 10) == kRed);   // the layer moved with it
    CHECK(doc.flatten().pixel(29, 19) == kRed);
    CHECK(doc.flatten().pixel(30, 20) == kWhite);
    CHECK(doc.flatten().pixel(19, 10) == kBlue);
  }

  CHECK(doc.layers().size() == 2);
  CHECK(doc.flatten() == uncached(doc));
}
