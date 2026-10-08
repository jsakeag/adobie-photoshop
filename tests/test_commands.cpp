#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/canvas_commands.h"
#include "ps/layer_commands.h"
#include "ps/pixel_commands.h"
#include "state_checks.h"

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

// A 4x3-tile document: test pattern background, small masked red layer.
Document sample() {
  Document doc(make_test_pattern(4 * kT, 3 * kT));
  Layer red = solid("red", {0, 0, 40, 20}, kRed);
  red.offset = {kT + 10, kT + 10};
  red.mask = MaskGrid(255);
  red.mask->fill({20, 0, 20, 20}, 0);
  doc.add_layer(1, red);
  return doc;
}

// A command paired with the direct Document calls it should be equal to.
struct Case {
  std::string name;
  std::function<std::unique_ptr<Command>()> make;
  std::function<void(Document&)> direct;
};

std::vector<Case> all_cases() {
  const Layer blue = solid("blue", {10, 10, 100, 80}, kBlue);
  const Rect box{kT - 8, 20, 30, 30};
  return {
      {"New Layer",
       [=] { return std::make_unique<AddLayerCommand>(1, blue); },
       [=](Document& d) { d.add_layer(1, blue); }},
      {"Delete Layer",
       [] { return std::make_unique<RemoveLayerCommand>(1); },
       [](Document& d) { d.remove_layer(1); }},
      {"Layer Order",
       [] { return std::make_unique<MoveLayerCommand>(1, 0); },
       [](Document& d) { d.move_layer(1, 0); }},
      {"Layer Opacity",
       [] { return std::make_unique<SetLayerOpacityCommand>(1, 0.4f); },
       [](Document& d) { d.set_layer_opacity(1, 0.4f); }},
      {"Hide Layer",
       [] { return std::make_unique<SetLayerVisibleCommand>(1, false); },
       [](Document& d) { d.set_layer_visible(1, false); }},
      {"Move",
       [] { return std::make_unique<SetLayerOffsetCommand>(1, Point{-5, 7}); },
       [](Document& d) { d.set_layer_offset(1, {-5, 7}); }},
      {"Blend Mode",
       [] {
         return std::make_unique<SetLayerBlendCommand>(1, BlendMode::Screen);
       },
       [](Document& d) { d.set_layer_blend(1, BlendMode::Screen); }},
      {"Add Layer Mask",
       [] { return std::make_unique<SetLayerMaskCommand>(0, MaskGrid(128)); },
       [](Document& d) { d.set_layer_mask(0, MaskGrid(128)); }},
      {"Remove Layer Mask",
       [] { return std::make_unique<SetLayerMaskCommand>(1, std::nullopt); },
       [](Document& d) { d.set_layer_mask(1, std::nullopt); }},
      {"Fill",
       [=] { return std::make_unique<FillCommand>(0, box, kBlue); },
       [=](Document& d) { d.edit_layer_pixels(0, box).fill(box, kBlue); }},
      {"Fill Mask",
       [] {
         return std::make_unique<FillMaskCommand>(1, Rect{0, 0, 10, 20}, 64);
       },
       [](Document& d) {
         d.edit_layer_mask(1, {0, 0, 10, 20}).fill({0, 0, 10, 20}, 64);
       }},
      {"Canvas Size",
       [] {
         return std::make_unique<ResizeCanvasCommand>(300, 100, Anchor::Top,
                                                      kBlack);
       },
       [](Document& d) { d.resize_canvas(300, 100, Anchor::Top, kBlack); }},
      {"Crop",
       [] { return std::make_unique<CropCommand>(Rect{kT, kT, 90, 60}); },
       [](Document& d) { d.crop({kT, kT, 90, 60}); }},
      {"Rotate Canvas",
       [] { return std::make_unique<RotateCanvasCommand>(Rotation::CCW90); },
       [](Document& d) { d.rotate(Rotation::CCW90); }},
      {"Flip Canvas",
       [] { return std::make_unique<FlipCanvasCommand>(FlipAxis::Horizontal); },
       [](Document& d) { d.flip_horizontal(); }},
      {"Flip Canvas",
       [] { return std::make_unique<FlipCanvasCommand>(FlipAxis::Vertical); },
       [](Document& d) { d.flip_vertical(); }},
  };
}

// Changes the document, then fails.
class HalfDoneCommand : public Command {
 public:
  std::string name() const override { return "Half Done"; }

 protected:
  void apply(Document& doc) override {
    doc.remove_layer(1);
    doc.edit_layer_pixels(0, {0, 0, 50, 50}).fill({0, 0, 50, 50}, kBlue);
    throw std::runtime_error("failed part way");
  }
};

}  // namespace

TEST_CASE("Every command applies, undoes and redoes", "[commands]") {
  for (const Case& c : all_cases()) {
    CAPTURE(c.name);
    Document doc = sample();
    doc.flatten();  // a warm cache, so stale tiles would show
    const DocumentState before = doc.snapshot();
    const Image picture_before = doc.flatten();

    Document expected = sample();
    c.direct(expected);

    const std::unique_ptr<Command> command = c.make();
    CHECK(command->name() == c.name);

    command->execute(doc);
    const DocumentState after = doc.snapshot();
    CHECK(same_state(after, expected.snapshot()));
    CHECK_FALSE(same_state(after, before));
    CHECK(cache_is_current(doc));

    command->undo(doc);
    CHECK(same_state(doc.snapshot(), before));
    CHECK(doc.flatten() == picture_before);
    CHECK(cache_is_current(doc));

    command->redo(doc);
    CHECK(same_state(doc.snapshot(), after));
    CHECK(doc.flatten() == expected.flatten());
    CHECK(cache_is_current(doc));

    command->undo(doc);
    CHECK(same_state(doc.snapshot(), before));
    CHECK(cache_is_current(doc));
  }
}

TEST_CASE("Command names follow their parameters", "[commands]") {
  CHECK(SetLayerVisibleCommand(0, true).name() == "Show Layer");
  CHECK(SetLayerVisibleCommand(0, false).name() == "Hide Layer");
  CHECK(SetLayerMaskCommand(0, MaskGrid(255)).name() == "Add Layer Mask");
  CHECK(SetLayerMaskCommand(0, std::nullopt).name() == "Remove Layer Mask");
}

TEST_CASE("Commands expose their parameters", "[commands]") {
  const FillCommand fill(2, {1, 2, 3, 4}, kRed);
  CHECK(fill.index == 2);
  CHECK(fill.area == Rect{1, 2, 3, 4});
  CHECK(fill.color == kRed);

  const ResizeCanvasCommand resize(30, 20, Anchor::Left, kBlack);
  CHECK(resize.width == 30);
  CHECK(resize.height == 20);
  CHECK(resize.anchor == Anchor::Left);
  CHECK(resize.fill == kBlack);
}

TEST_CASE("Commands must be used in order", "[commands]") {
  Document doc = sample();
  const DocumentState before = doc.snapshot();
  FillCommand fill(0, {0, 0, 10, 10}, kBlue);

  CHECK_THROWS_AS(fill.undo(doc), std::logic_error);
  CHECK_THROWS_AS(fill.redo(doc), std::logic_error);
  CHECK(same_state(doc.snapshot(), before));

  fill.execute(doc);
  const DocumentState after = doc.snapshot();
  CHECK_THROWS_AS(fill.execute(doc), std::logic_error);
  CHECK_THROWS_AS(fill.redo(doc), std::logic_error);
  CHECK(same_state(doc.snapshot(), after));

  fill.undo(doc);
  CHECK_THROWS_AS(fill.undo(doc), std::logic_error);
  CHECK_THROWS_AS(fill.execute(doc), std::logic_error);
  CHECK(same_state(doc.snapshot(), before));
}

TEST_CASE("A failed command leaves the document as it was", "[commands]") {
  Document doc = sample();
  doc.flatten();
  const DocumentState before = doc.snapshot();

  SECTION("rejected up front") {
    RemoveLayerCommand remove(7);
    CHECK_THROWS_AS(remove.execute(doc), std::out_of_range);
    CHECK_THROWS_AS(remove.undo(doc), std::logic_error);  // nothing to undo
  }
  SECTION("no mask to fill") {
    FillMaskCommand fill(0, {0, 0, 5, 5}, 0);
    CHECK_THROWS_AS(fill.execute(doc), std::logic_error);
  }
  SECTION("empty canvas") {
    ResizeCanvasCommand resize(0, 10, Anchor::Center, kWhite);
    CHECK_THROWS_AS(resize.execute(doc), std::invalid_argument);
  }
  SECTION("failure after changing the document") {
    HalfDoneCommand half;
    CHECK_THROWS_AS(half.execute(doc), std::runtime_error);
    CHECK_THROWS_AS(half.undo(doc), std::logic_error);
  }

  CHECK(same_state(doc.snapshot(), before));
  CHECK(cache_is_current(doc));
}

TEST_CASE("Undo keeps only the tiles an edit changed", "[commands]") {
  Document doc = sample();
  const TileGrid before = doc.layers().at(0).pixels;  // shares all 12 tiles
  REQUIRE(before.tile_count() == 12);

  FillCommand fill(0, {5, 5, 20, 20}, kBlue);  // inside one tile
  fill.execute(doc);
  CHECK(doc.layers().at(0).pixels.shared_tiles(before) == 11);
  const TileGrid after = doc.layers().at(0).pixels;

  // Undo and redo put back the very same tiles, not copies of them.
  fill.undo(doc);
  CHECK(doc.layers().at(0).pixels.shared_tiles(before) == 12);
  fill.redo(doc);
  CHECK(doc.layers().at(0).pixels.shared_tiles(after) == 12);
  CHECK(doc.layers().at(0).pixels.shared_tiles(before) == 11);
}

TEST_CASE("Undoing a small edit recomposites only its tiles", "[commands]") {
  Document doc = sample();
  doc.flatten();
  const std::size_t start = doc.compositor().tiles_composited();

  FillCommand fill(0, {5, 5, 20, 20}, kBlue);
  fill.execute(doc);
  doc.flatten();
  CHECK(doc.compositor().tiles_composited() == start + 1);
  fill.undo(doc);
  doc.flatten();
  CHECK(doc.compositor().tiles_composited() == start + 2);
  fill.redo(doc);
  doc.flatten();
  CHECK(doc.compositor().tiles_composited() == start + 3);
}
