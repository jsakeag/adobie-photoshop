// The project's definition of done, end to end, through the same commands
// the GUI issues: open a PNG, add layers with blend modes and masks, make
// selections, paint, apply adjustments and filters, transform, undo the
// last action, and save and reload in the native format.

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ps/adjust_commands.h"
#include "ps/brush.h"
#include "ps/editor.h"
#include "ps/filter_commands.h"
#include "ps/image_io.h"
#include "ps/layer_commands.h"
#include "ps/native_format.h"
#include "ps/paint_commands.h"
#include "ps/selection_commands.h"
#include "ps/transform_commands.h"
#include "state_checks.h"

using namespace ps;

TEST_CASE("The definition of done, through commands", "[done]") {
  // Open a PNG.
  const std::string fixtures = std::string(PS_GOLDEN_DIR) + "/../fixtures/";
  Editor editor(Document(load_image(fixtures + "selftest_base.png")));
  const auto run = [&](std::unique_ptr<Command> c) { editor.execute(std::move(c)); };
  const Document& doc = editor.document();
  REQUIRE(doc.width() == 640);

  // Add layers with blend modes and masks.
  Layer overlay;
  overlay.name = "Overlay";
  overlay.pixels.write(load_image(fixtures + "selftest_layer.png"), {0, 0});
  run(std::make_unique<AddLayerCommand>(1, overlay));
  run(std::make_unique<SetLayerOffsetCommand>(1, Point{170, 110}));
  run(std::make_unique<SetLayerBlendCommand>(1, BlendMode::Multiply));
  run(std::make_unique<SetLayerMaskCommand>(1, MaskGrid(255)));
  run(std::make_unique<AddLayerCommand>(2, Layer{"Paint", {}, {}, 1.0f, true,
                                                 BlendMode::Screen, std::nullopt}));

  // Make selections and paint inside them.
  run(std::make_unique<SelectEllipseCommand>(Rect{60, 60, 200, 140}, SelectionOp::Replace));
  run(std::make_unique<SelectRectCommand>(Rect{200, 100, 120, 60}, SelectionOp::Add));
  run(std::make_unique<FeatherSelectionCommand>(4.0f));
  Stroke stroke(BrushSettings{30.0f, 0.5f, 1.0f, 1.0f, 0.25f});
  for (int x = 40; x <= 360; x += 20) stroke.add_point({double(x), 130.0 + (x % 40)});
  run(std::make_unique<PaintStrokeCommand>(2, PaintTarget::Pixels, stroke.coverage(),
                                           Color{40, 200, 255, 255}, 0.8f, false));
  run(std::make_unique<PaintStrokeCommand>(1, PaintTarget::Mask, rect_mask({0, 0, 150, 200}),
                                           kBlack, 1.0f, false));
  run(std::make_unique<DeselectCommand>());
  run(std::make_unique<BucketFillCommand>(0, PaintTarget::Pixels, Point{20, 400},
                                          Color{250, 210, 60, 255}, 10, 1.0f));

  // Apply adjustments and filters.
  run(std::make_unique<AdjustCommand>(0, HueSaturation{20, 10, 0}));
  run(std::make_unique<AdjustCommand>(0, Levels{10, 245, 1.2f, 0, 255}));
  run(std::make_unique<FilterCommand>(2, GaussianBlur{2.0f}));
  run(std::make_unique<FilterCommand>(0, UnsharpMask{0.8f, 1.5f, 2}));

  // Transform.
  run(std::make_unique<TransformLayerCommand>(
      1, scale_rotate_about({320, 210}, 0.8, 0.8, -15), Interpolation::Bicubic));
  const DocumentState before_last = doc.snapshot();
  run(std::make_unique<ImageSizeCommand>(480, 315, Interpolation::Bicubic));
  CHECK(doc.width() == 480);

  // Undo the last action.
  REQUIRE(editor.undo());
  CHECK(same_state(doc.snapshot(), before_last));
  CHECK(cache_is_current(doc));

  // Save and reload in the native format.
  const std::string path = std::string(PS_TEST_OUTPUT_DIR) + "/definition_of_done.mps";
  save_native(doc, path);
  const Document reloaded = load_native(path);
  DocumentState expected = doc.snapshot();
  expected.selection.reset();  // selections are not saved
  CHECK(same_state(reloaded.snapshot(), expected));
  CHECK(reloaded.flatten() == doc.flatten());
  CHECK(reloaded.layers().size() == 3);
  CHECK(reloaded.layers().at(1).blend == BlendMode::Multiply);
  CHECK(reloaded.layers().at(1).mask.has_value());
}
