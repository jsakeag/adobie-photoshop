#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/brush.h"
#include "ps/adjust_commands.h"
#include "ps/canvas_commands.h"
#include "ps/layer_commands.h"
#include "ps/paint_commands.h"
#include "ps/pixel_commands.h"
#include "ps/selection_commands.h"
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

// A 4x3-tile document: test pattern background, small masked red layer,
// and a rectangular selection.
Document sample() {
  Document doc(make_test_pattern(4 * kT, 3 * kT));
  Layer red = solid("red", {0, 0, 40, 20}, kRed);
  red.offset = {kT + 10, kT + 10};
  red.mask = MaskGrid(255);
  red.mask->fill({20, 0, 20, 20}, 0);
  doc.add_layer(1, red);
  doc.set_selection(rect_mask({20, 20, 2 * kT, kT}));
  return doc;
}

// Brush-like coverage crossing the selection's edge.
MaskGrid streak() {
  Stroke stroke(BrushSettings{});
  stroke.add_point({10, 30});
  stroke.add_point({kT + 40, kT + 25});
  return stroke.coverage();
}

// What the bucket does, worked out directly.
void bucket(Document& d, Point seed) {
  const Image pixels = d.layers().at(0).pixels.read(d.bounds());
  paint_coverage(d, 0, PaintTarget::Pixels, flood_mask(pixels, {0, 0}, seed, 12),
                 kBlue, 1.0f, false);
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
      {"Rectangular Marquee",
       [] {
         return std::make_unique<SelectRectCommand>(Rect{5, 5, 30, 30},
                                                    SelectionOp::Add);
       },
       [](Document& d) {
         d.set_selection(combine_selection(d.selection(), rect_mask({5, 5, 30, 30}),
                                           SelectionOp::Add));
       }},
      {"Elliptical Marquee",
       [] {
         return std::make_unique<SelectEllipseCommand>(Rect{5, 5, 60, 30},
                                                       SelectionOp::Replace);
       },
       [](Document& d) { d.set_selection(ellipse_mask({5, 5, 60, 30})); }},
      {"Lasso",
       [] {
         return std::make_unique<SelectPolygonCommand>(
             std::vector<Vec2>{{0, 0}, {90, 10}, {40, 70}}, SelectionOp::Subtract);
       },
       [](Document& d) {
         d.set_selection(combine_selection(
             d.selection(), polygon_mask({{0, 0}, {90, 10}, {40, 70}}),
             SelectionOp::Subtract));
       }},
      {"Magic Wand",
       [] {
         return std::make_unique<MagicWandCommand>(1, Point{kT + 15, kT + 15}, 0,
                                                   SelectionOp::Replace);
       },
       [](Document& d) { d.set_selection(rect_mask({kT + 10, kT + 10, 40, 20})); }},
      {"Select All", [] { return std::make_unique<SelectAllCommand>(); },
       [](Document& d) { d.set_selection(rect_mask(d.bounds())); }},
      {"Deselect", [] { return std::make_unique<DeselectCommand>(); },
       [](Document& d) { d.set_selection(std::nullopt); }},
      {"Inverse", [] { return std::make_unique<InvertSelectionCommand>(); },
       [](Document& d) { d.set_selection(inverted(*d.selection())); }},
      {"Feather", [] { return std::make_unique<FeatherSelectionCommand>(5.0f); },
       [](Document& d) { d.set_selection(feathered(*d.selection(), 5.0f)); }},
      {"Paintbrush",
       [] {
         return std::make_unique<PaintStrokeCommand>(0, PaintTarget::Pixels,
                                                     streak(), kBlue, 0.7f, false);
       },
       [](Document& d) {
         paint_coverage(d, 0, PaintTarget::Pixels, streak(), kBlue, 0.7f, false);
       }},
      {"Eraser",
       [] {
         return std::make_unique<PaintStrokeCommand>(0, PaintTarget::Pixels,
                                                     streak(), kBlue, 1.0f, true);
       },
       [](Document& d) {
         paint_coverage(d, 0, PaintTarget::Pixels, streak(), kBlue, 1.0f, true);
       }},
      {"Paintbrush",
       [] {
         return std::make_unique<PaintStrokeCommand>(1, PaintTarget::Mask,
                                                     rect_mask({0, 0, 999, 999}),
                                                     kBlack, 1.0f, false);
       },
       [](Document& d) {
         paint_coverage(d, 1, PaintTarget::Mask, rect_mask({0, 0, 999, 999}),
                        kBlack, 1.0f, false);
       }},
      {"Paint Bucket",
       [] {
         return std::make_unique<BucketFillCommand>(0, PaintTarget::Pixels,
                                                    Point{30, 30}, kBlue, 12, 1.0f);
       },
       [](Document& d) { bucket(d, {30, 30}); }},
      {"Levels",
       [] { return std::make_unique<AdjustCommand>(0, Levels{30, 220, 1.3f, 0, 255}); },
       [](Document& d) {
         // Outside the selection nothing changes; inside, the levels table.
         const Lut lut = levels_lut({30, 220, 1.3f, 0, 255});
         const Rect sel{20, 20, 2 * kT, kT};
         Image image = d.layers().at(0).pixels.read(sel);
         for (int y = 0; y < sel.height; ++y) {
           for (int x = 0; x < sel.width; ++x) {
             const Color c = image.pixel(x, y);
             image.set_pixel(x, y, {lut[c.r], lut[c.g], lut[c.b], c.a});
           }
         }
         d.edit_layer_pixels(0, sel).write(image, {sel.x, sel.y});
       }},
      {"Invert", [] { return std::make_unique<AdjustCommand>(1, Invert{}); },
       [](Document& d) {
         // The selection ends at document row 84, ten rows into the layer.
         d.edit_layer_pixels(1, {0, 0, 40, 10})
             .fill({0, 0, 40, 10}, {0, 255, 255, 255});
       }},
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

TEST_CASE("Selection commands need a selection where it matters",
          "[commands][selection]") {
  Document doc(40, 40);
  const DocumentState before = doc.snapshot();
  InvertSelectionCommand invert;
  FeatherSelectionCommand feather(3.0f);
  CHECK_THROWS_AS(invert.execute(doc), std::logic_error);
  CHECK_THROWS_AS(feather.execute(doc), std::logic_error);
  CHECK(same_state(doc.snapshot(), before));
}

TEST_CASE("Seeds outside the canvas change nothing", "[commands][selection]") {
  Document doc = sample();
  const DocumentState before = doc.snapshot();
  MagicWandCommand wand(0, {-1, 5}, 255, SelectionOp::Replace);
  wand.execute(doc);
  BucketFillCommand fill(0, PaintTarget::Pixels, {5, 3 * kT}, kBlue, 255, 1.0f);
  fill.execute(doc);
  CHECK(same_state(doc.snapshot(), before));
}

TEST_CASE("The selection survives undo and canvas changes deselect",
          "[commands][selection]") {
  Document doc = sample();
  REQUIRE(doc.selection());
  RotateCanvasCommand rotate(Rotation::CW90);
  rotate.execute(doc);
  CHECK_FALSE(doc.selection());
  rotate.undo(doc);
  REQUIRE(doc.selection());
  CHECK(*doc.selection() == rect_mask({20, 20, 2 * kT, kT}));
}

TEST_CASE("Changing the selection does not recomposite", "[commands][selection]") {
  Document doc = sample();
  doc.flatten();
  const std::size_t before = doc.compositor().tiles_composited();
  SelectAllCommand all;
  all.execute(doc);
  all.undo(doc);
  doc.flatten();
  CHECK(doc.compositor().tiles_composited() == before);
}
