#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/document.h"
#include "state_checks.h"

using namespace ps;

namespace {

constexpr int kT = kTileSize;
const Color kRed{255, 0, 0, 255};
const Color kBlue{0, 0, 255, 255};

// A 4x3-tile document: test pattern background, small masked red layer.
Document sample() {
  Document doc(make_test_pattern(4 * kT, 3 * kT));
  Layer red;
  red.name = "red";
  red.pixels.fill({0, 0, 40, 20}, kRed);
  red.offset = {kT + 10, kT + 10};
  red.mask = MaskGrid(255);
  red.mask->fill({20, 0, 20, 20}, 0);
  doc.add_layer(1, red);
  return doc;
}

}  // namespace

TEST_CASE("A snapshot shares the document's tiles", "[state]") {
  const Document doc = sample();
  const DocumentState state = doc.snapshot();
  CHECK(state.width == 4 * kT);
  CHECK(state.height == 3 * kT);
  REQUIRE(state.layers.size() == 2);
  CHECK(state.layers.at(0).pixels.shared_tiles(doc.layers().at(0).pixels) == 12);
  CHECK(state.layers.at(1).mask->shared_tiles(*doc.layers().at(1).mask) == 1);
  CHECK(same_state(state, doc.snapshot()));
}

TEST_CASE("A snapshot is unaffected by later edits", "[state]") {
  Document doc = sample();
  const Image before = doc.flatten();
  const DocumentState state = doc.snapshot();

  doc.edit_layer_pixels(0, {0, 0, 30, 30}).fill({0, 0, 30, 30}, kBlue);
  doc.set_layer_opacity(1, 0.25f);
  doc.edit_layer_mask(1, {0, 0, 5, 5}).fill({0, 0, 5, 5}, 0);

  CHECK_FALSE(same_state(state, doc.snapshot()));
  Document other(1, 1);
  other.restore(state);
  CHECK(other.flatten() == before);
}

TEST_CASE("Restore brings back every kind of change", "[state]") {
  Document doc = sample();
  const Image before = doc.flatten();
  const DocumentState state = doc.snapshot();

  SECTION("pixels") {
    doc.edit_layer_pixels(0, {5, 5, 100, 100}).fill({5, 5, 100, 100}, kBlue);
  }
  SECTION("mask content") {
    doc.edit_layer_mask(1, {0, 0, 40, 20}).fill({0, 0, 40, 20}, 128);
  }
  SECTION("mask removed") { doc.set_layer_mask(1, std::nullopt); }
  SECTION("mask background") { doc.set_layer_mask(1, MaskGrid(0)); }
  SECTION("opacity") { doc.set_layer_opacity(1, 0.3f); }
  SECTION("visibility") { doc.set_layer_visible(1, false); }
  SECTION("blend mode") { doc.set_layer_blend(1, BlendMode::Difference); }
  SECTION("offset") { doc.set_layer_offset(1, {3, 3}); }
  SECTION("layer added") {
    Layer blue;
    blue.pixels.fill({0, 0, 50, 50}, kBlue);
    doc.add_layer(2, blue);
  }
  SECTION("layer removed") { doc.remove_layer(1); }
  SECTION("layers reordered") { doc.move_layer(1, 0); }
  SECTION("canvas resized") { doc.resize_canvas(100, 50, Anchor::TopLeft); }
  SECTION("canvas rotated") { doc.rotate(Rotation::CW90); }
  SECTION("canvas cropped") { doc.crop({kT, kT, 60, 60}); }
  SECTION("nothing") {}

  doc.flatten();  // fill the cache with the changed picture
  doc.restore(state);
  CHECK(same_state(doc.snapshot(), state));
  CHECK(doc.flatten() == before);
  CHECK(cache_is_current(doc));
}

TEST_CASE("Restore recomposites only what differs", "[state]") {
  Document doc = sample();  // 12 tiles; the red layer sits in tile (1, 1)
  doc.flatten();
  const DocumentState state = doc.snapshot();
  std::size_t count = doc.compositor().tiles_composited();
  CHECK(count == 12);

  SECTION("an identical state costs nothing") {
    doc.restore(state);
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == count);
  }

  SECTION("a small pixel edit costs its tiles") {
    doc.edit_layer_pixels(0, {kT - 2, 5, 4, 4}).fill({kT - 2, 5, 4, 4}, kBlue);
    doc.flatten();
    count += 2;
    CHECK(doc.compositor().tiles_composited() == count);
    doc.restore(state);
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == count + 2);
  }

  SECTION("a mask edit costs its tiles") {
    doc.edit_layer_mask(1, {0, 0, 4, 4}).fill({0, 0, 4, 4}, 0);
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == count + 1);
    doc.restore(state);
    doc.flatten();
    // Restore works per changed mask tile; that tile, offset by the layer's
    // (10, 10), overlaps four document tiles.
    CHECK(doc.compositor().tiles_composited() == count + 1 + 4);
  }

  SECTION("a property change costs the layer's tiles") {
    doc.set_layer_opacity(1, 0.5f);
    doc.flatten();
    doc.restore(state);
    doc.flatten();
    // The layer's one 64x64 tile, offset by (10, 10), overlaps four tiles.
    CHECK(doc.compositor().tiles_composited() == count + 8);
  }

  SECTION("a change in the layer list costs everything") {
    doc.remove_layer(1);
    doc.flatten();
    const std::size_t after_remove = doc.compositor().tiles_composited();
    doc.restore(state);
    doc.flatten();
    CHECK(doc.compositor().tiles_composited() == after_remove + 12);
  }

  CHECK(cache_is_current(doc));
}

TEST_CASE("Restore rejects a state with no canvas", "[state]") {
  Document doc = sample();
  const DocumentState before = doc.snapshot();
  CHECK_THROWS_AS(doc.restore(DocumentState{}), std::invalid_argument);
  CHECK(same_state(doc.snapshot(), before));
}
