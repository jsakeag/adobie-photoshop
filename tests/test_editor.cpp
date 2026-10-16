#include <memory>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/canvas_commands.h"
#include "ps/editor.h"
#include "ps/layer_commands.h"
#include "ps/pixel_commands.h"
#include "state_checks.h"

using namespace ps;

namespace {

const Color kRed{255, 0, 0, 255};
const Color kBlue{0, 0, 255, 255};

std::unique_ptr<Command> fill(Rect area, Color c) {
  return std::make_unique<FillCommand>(0, area, c);
}

}  // namespace

TEST_CASE("A new editor has nothing to undo", "[editor]") {
  Editor editor(Document(40, 30));
  CHECK(editor.document().width() == 40);
  CHECK_FALSE(editor.can_undo());
  CHECK_FALSE(editor.can_redo());
  CHECK(editor.undo_name().empty());
  CHECK(editor.redo_name().empty());
  CHECK_FALSE(editor.undo());
  CHECK_FALSE(editor.redo());
  CHECK(editor.document().flatten() == Image(40, 30, kWhite));
}

TEST_CASE("Editor undo and redo toggle the last command", "[editor]") {
  Editor editor(Document(40, 30));
  const DocumentState before = editor.document().snapshot();

  editor.execute(fill({0, 0, 10, 10}, kRed));
  const DocumentState after = editor.document().snapshot();
  CHECK(editor.document().flatten().pixel(5, 5) == kRed);
  CHECK(editor.can_undo());
  CHECK_FALSE(editor.can_redo());
  CHECK(editor.undo_name() == "Fill");
  CHECK(editor.redo_name().empty());
  CHECK_FALSE(editor.redo());

  CHECK(editor.undo());
  CHECK(same_state(editor.document().snapshot(), before));
  CHECK_FALSE(editor.can_undo());
  CHECK(editor.can_redo());
  CHECK(editor.undo_name().empty());
  CHECK(editor.redo_name() == "Fill");
  CHECK_FALSE(editor.undo());

  CHECK(editor.redo());
  CHECK(same_state(editor.document().snapshot(), after));
  CHECK(editor.can_undo());

  // Back and forth as often as you like.
  for (int i = 0; i < 3; ++i) {
    CHECK(editor.undo());
    CHECK(same_state(editor.document().snapshot(), before));
    CHECK(editor.redo());
    CHECK(same_state(editor.document().snapshot(), after));
  }
  CHECK(cache_is_current(editor.document()));
}

TEST_CASE("Editor undo is single-level", "[editor]") {
  Editor editor(Document(40, 30));
  editor.execute(fill({0, 0, 10, 10}, kRed));
  const DocumentState after_first = editor.document().snapshot();
  editor.execute(std::make_unique<SetLayerOpacityCommand>(0, 0.5f));
  CHECK(editor.undo_name() == "Layer Opacity");

  CHECK(editor.undo());
  CHECK(same_state(editor.document().snapshot(), after_first));
  CHECK_FALSE(editor.undo());  // the fill is out of reach
  CHECK(editor.document().flatten().pixel(5, 5) == kRed);
}

TEST_CASE("A new command replaces the one that could be redone", "[editor]") {
  Editor editor(Document(40, 30));
  editor.execute(fill({0, 0, 10, 10}, kRed));
  editor.undo();
  REQUIRE(editor.can_redo());

  editor.execute(fill({20, 0, 10, 10}, kBlue));
  CHECK_FALSE(editor.can_redo());
  CHECK(editor.can_undo());
  CHECK(editor.document().flatten().pixel(5, 5) == kWhite);  // red stays undone
  CHECK(editor.document().flatten().pixel(25, 5) == kBlue);

  editor.undo();
  CHECK(editor.document().flatten() == Image(40, 30, kWhite));
}

TEST_CASE("A failed command does not disturb the editor", "[editor]") {
  Editor editor(Document(40, 30));
  editor.execute(fill({0, 0, 10, 10}, kRed));
  const DocumentState state = editor.document().snapshot();

  CHECK_THROWS_AS(editor.execute(std::make_unique<RemoveLayerCommand>(5)),
                  std::out_of_range);
  CHECK_THROWS_AS(editor.execute(nullptr), std::invalid_argument);
  CHECK(same_state(editor.document().snapshot(), state));
  CHECK(editor.undo_name() == "Fill");  // the earlier command is still there

  CHECK(editor.undo());
  CHECK(editor.document().flatten() == Image(40, 30, kWhite));
}

TEST_CASE("An editing session driven only by commands", "[editor]") {
  // The same steps made through commands and through Document directly must
  // give the same document.
  const Image photo = make_test_pattern(160, 120);
  Layer tint;
  tint.name = "tint";
  tint.pixels.fill({0, 0, 100, 80}, {255, 200, 0, 255});
  MaskGrid mask(255);
  mask.fill({0, 0, 50, 80}, 0);

  Editor editor{Document(photo)};
  editor.execute(std::make_unique<AddLayerCommand>(1, tint));
  editor.execute(std::make_unique<SetLayerOffsetCommand>(1, Point{30, 20}));
  editor.execute(std::make_unique<SetLayerBlendCommand>(1, BlendMode::Multiply));
  editor.execute(std::make_unique<SetLayerMaskCommand>(1, mask));
  editor.execute(std::make_unique<FillMaskCommand>(1, Rect{0, 0, 20, 20}, 128));
  editor.execute(std::make_unique<SetLayerOpacityCommand>(1, 0.75f));
  editor.execute(std::make_unique<FillCommand>(0, Rect{0, 0, 16, 16}, kBlue));
  editor.execute(std::make_unique<RotateCanvasCommand>(Rotation::CW90));

  Document direct(photo);
  direct.add_layer(1, tint);
  direct.set_layer_offset(1, {30, 20});
  direct.set_layer_blend(1, BlendMode::Multiply);
  direct.set_layer_mask(1, mask);
  direct.edit_layer_mask(1, {0, 0, 20, 20}).fill({0, 0, 20, 20}, 128);
  direct.set_layer_opacity(1, 0.75f);
  direct.edit_layer_pixels(0, {0, 0, 16, 16}).fill({0, 0, 16, 16}, kBlue);
  const DocumentState before_rotate = direct.snapshot();
  direct.rotate(Rotation::CW90);

  CHECK(same_state(editor.document().snapshot(), direct.snapshot()));
  CHECK(editor.document().flatten() == direct.flatten());
  CHECK(editor.document().width() == 120);

  CHECK(editor.undo());
  CHECK(same_state(editor.document().snapshot(), before_rotate));
  CHECK(editor.document().width() == 160);
  CHECK(editor.redo());
  CHECK(editor.document().flatten() == direct.flatten());
  CHECK(cache_is_current(editor.document()));
}

TEST_CASE("Editor toggle_undo flips between undo and redo", "[editor]") {
  Editor editor(Document(40, 30));
  CHECK_FALSE(editor.toggle_undo());

  editor.execute(fill({0, 0, 10, 10}, kRed));
  const DocumentState after = editor.document().snapshot();

  CHECK(editor.toggle_undo());  // undo
  CHECK(editor.can_redo());
  CHECK(editor.document().flatten() == Image(40, 30, kWhite));
  CHECK(editor.toggle_undo());  // redo
  CHECK(editor.can_undo());
  CHECK(same_state(editor.document().snapshot(), after));
}
