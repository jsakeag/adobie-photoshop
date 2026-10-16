#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/paint_commands.h"
#include "ps/selection_commands.h"
#include "ps/tools.h"
#include "state_checks.h"

using namespace ps;

namespace {

const Color kRed{255, 0, 0, 255};

Document sample() { return Document(make_test_pattern(120, 80)); }

// The document after the command, or the unchanged document if none.
DocumentState after(std::unique_ptr<Command> command) {
  Document doc = sample();
  if (command) command->execute(doc);
  return doc.snapshot();
}

std::unique_ptr<Command> drag_gesture(Tool& tool, const std::vector<Vec2>& points,
                                      const ToolSettings& settings = {}) {
  tool.press(points.front(), settings);
  for (std::size_t i = 1; i + 1 < points.size(); ++i) tool.drag(points[i]);
  return tool.release(points.back());
}

}  // namespace

TEST_CASE("Every tool but the eyedropper has a Tool", "[tools]") {
  std::vector<std::string> names;
  for (ToolKind kind : kToolKinds) {
    CAPTURE(tool_name(kind));
    CHECK((make_tool(kind) == nullptr) == (kind == ToolKind::Eyedropper));
    for (const std::string& n : names) CHECK(n != tool_name(kind));
    names.push_back(tool_name(kind));
  }
}

TEST_CASE("Marquee tools select the dragged rectangle", "[tools]") {
  auto tool = make_tool(ToolKind::RectMarquee);
  auto command = drag_gesture(*tool, {{10.2, 10.7}, {20, 30}, {30.9, 25.1}});
  REQUIRE(command);
  CHECK(command->name() == "Rectangular Marquee");
  CHECK(*after(std::move(command)).selection == rect_mask({10, 10, 20, 15}));
  CHECK_FALSE(tool->active());
  CHECK(tool->command() == nullptr);

  SECTION("dragging up and left gives the same rectangle") {
    auto reverse = drag_gesture(*tool, {{30.9, 25.1}, {10.2, 10.7}});
    CHECK(*after(std::move(reverse)).selection == rect_mask({10, 10, 20, 15}));
  }
  SECTION("the ellipse tool selects the ellipse in that rectangle") {
    auto ellipse = make_tool(ToolKind::EllipseMarquee);
    auto c = drag_gesture(*ellipse, {{10, 10}, {30, 25}});
    CHECK(c->name() == "Elliptical Marquee");
    CHECK(*after(std::move(c)).selection == ellipse_mask({10, 10, 20, 15}));
  }
  SECTION("a click deselects") {
    auto click = drag_gesture(*tool, {{40, 40}, {40.3, 40.2}});
    REQUIRE(click);
    CHECK(click->name() == "Deselect");
  }
  SECTION("a click while adding does nothing") {
    ToolSettings adding;
    adding.op = SelectionOp::Add;
    CHECK(drag_gesture(*tool, {{40, 40}, {40, 40}}, adding) == nullptr);
  }
  SECTION("the selection mode is passed on") {
    ToolSettings subtract;
    subtract.op = SelectionOp::Subtract;
    auto c = drag_gesture(*tool, {{0, 0}, {10, 10}}, subtract);
    CHECK(static_cast<SelectRectCommand&>(*c).op == SelectionOp::Subtract);
  }
}

TEST_CASE("The lasso selects the traced outline", "[tools]") {
  auto tool = make_tool(ToolKind::Lasso);
  auto command = drag_gesture(*tool, {{10, 10}, {30, 10}, {30, 20}, {10, 20}});
  REQUIRE(command);
  CHECK(command->name() == "Lasso");
  CHECK(*after(std::move(command)).selection == rect_mask({10, 10, 20, 10}));
  CHECK(drag_gesture(*tool, {{5, 5}, {9, 9}})->name() == "Deselect");
}

TEST_CASE("The magic wand acts where it was pressed", "[tools]") {
  auto tool = make_tool(ToolKind::MagicWand);
  ToolSettings s;
  s.tolerance = 7;
  auto command = drag_gesture(*tool, {{12.9, 8.1}, {50, 50}}, s);
  REQUIRE(command);
  const auto& wand = static_cast<MagicWandCommand&>(*command);
  CHECK(wand.seed.x == 12);
  CHECK(wand.seed.y == 8);
  CHECK(wand.tolerance == 7);
  CHECK(wand.index == 0);
}

TEST_CASE("Brush and eraser paint the stroke they were dragged along",
          "[tools]") {
  ToolSettings s;
  s.color = kRed;
  s.brush.diameter = 6;
  s.brush.opacity = 0.8f;
  const std::vector<Vec2> path{{10, 10}, {30, 12}, {50, 40}};

  auto brush = make_tool(ToolKind::Brush);
  auto command = drag_gesture(*brush, path, s);
  REQUIRE(command);
  CHECK(command->name() == "Paintbrush");
  const auto& paint = static_cast<PaintStrokeCommand&>(*command);
  Stroke expected(s.brush);
  for (const Vec2& p : path) expected.add_point(p);
  CHECK(paint.coverage == expected.coverage());
  CHECK(paint.color == kRed);
  CHECK(paint.opacity == 0.8f);
  CHECK_FALSE(paint.erase);

  auto eraser = make_tool(ToolKind::Eraser);
  auto erase = drag_gesture(*eraser, path, s);
  CHECK(erase->name() == "Eraser");
  CHECK(static_cast<PaintStrokeCommand&>(*erase).erase);
}

TEST_CASE("The preview grows with the gesture", "[tools]") {
  auto brush = make_tool(ToolKind::Brush);
  CHECK(brush->command() == nullptr);
  brush->press({10, 10}, {});
  const MaskGrid start =
      static_cast<PaintStrokeCommand&>(*brush->command()).coverage;
  brush->drag({60, 10});
  const MaskGrid longer =
      static_cast<PaintStrokeCommand&>(*brush->command()).coverage;
  CHECK(start.value(50, 10) == 0);
  CHECK(longer.value(50, 10) == 255);
}

TEST_CASE("The bucket fills from where it was pressed", "[tools]") {
  auto tool = make_tool(ToolKind::Bucket);
  ToolSettings s;
  s.color = kRed;
  s.tolerance = 3;
  s.target = PaintTarget::Mask;
  auto command = drag_gesture(*tool, {{4.5, 6.5}, {4.5, 6.5}}, s);
  const auto& fill = static_cast<BucketFillCommand&>(*command);
  CHECK(fill.seed.x == 4);
  CHECK(fill.seed.y == 6);
  CHECK(fill.tolerance == 3);
  CHECK(fill.target == PaintTarget::Mask);
}

TEST_CASE("Gestures can be cancelled and restarted", "[tools]") {
  auto tool = make_tool(ToolKind::RectMarquee);
  tool->drag({5, 5});  // no press: ignored
  CHECK(tool->release({9, 9}) == nullptr);

  tool->press({0, 0}, {});
  tool->cancel();
  CHECK_FALSE(tool->active());
  CHECK(tool->release({50, 50}) == nullptr);

  tool->press({0, 0}, {});
  tool->press({20, 20}, {});  // a new press starts over
  auto command = tool->release({30, 30});
  CHECK(*after(std::move(command)).selection == rect_mask({20, 20, 10, 10}));
}

TEST_CASE("Tool settings are captured when the gesture starts", "[tools]") {
  auto tool = make_tool(ToolKind::Brush);
  ToolSettings s;
  s.color = kRed;
  tool->press({10, 10}, s);
  s.color = kBlack;  // too late to affect this stroke
  auto command = tool->release({20, 10});
  CHECK(static_cast<PaintStrokeCommand&>(*command).color == kRed);
}
