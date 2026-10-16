#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/selection.h"

using namespace ps;

namespace {

int count_at_least(const MaskGrid& mask, Rect area, int threshold) {
  int n = 0;
  for (std::uint8_t v : mask.read(area)) n += v >= threshold;
  return n;
}

}  // namespace

TEST_CASE("Rectangle selection", "[selection]") {
  const MaskGrid mask = rect_mask({10, 20, 30, 5});
  CHECK(mask.background() == 0);
  CHECK(mask.value(10, 20) == 255);
  CHECK(mask.value(39, 24) == 255);
  CHECK(mask.value(40, 24) == 0);
  CHECK(mask.value(39, 25) == 0);
  CHECK(mask.value(9, 20) == 0);
  CHECK(count_at_least(mask, {0, 0, 100, 100}, 1) == 150);
  CHECK(rect_mask({5, 5, 0, 10}).empty());
}

TEST_CASE("Ellipse selection", "[selection]") {
  const MaskGrid mask = ellipse_mask({0, 0, 20, 10});
  CHECK(mask.value(10, 5) == 255);  // centre
  CHECK(mask.value(0, 0) == 0);     // corners are outside
  CHECK(mask.value(19, 9) == 0);
  CHECK(mask.value(0, 5) == 255);   // ends of the long axis
  CHECK(mask.value(19, 5) == 255);
  CHECK(mask.value(20, 5) == 0);
  // Symmetric both ways.
  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 20; ++x) {
      CAPTURE(x, y);
      CHECK(mask.value(x, y) == mask.value(19 - x, y));
      CHECK(mask.value(x, y) == mask.value(x, 9 - y));
    }
  }
  // About pi * 10 * 5 pixels.
  const int area = count_at_least(mask, {0, 0, 20, 10}, 255);
  CHECK(area > 150);
  CHECK(area < 165);
  CHECK(ellipse_mask({0, 0, 0, 5}).empty());
}

TEST_CASE("Polygon selection", "[selection]") {
  SECTION("a square drawn as a polygon matches the rectangle") {
    const MaskGrid square = polygon_mask({{10, 10}, {30, 10}, {30, 20}, {10, 20}});
    CHECK(square == rect_mask({10, 10, 20, 10}));
  }
  SECTION("a triangle") {
    const MaskGrid tri = polygon_mask({{0, 0}, {20, 0}, {0, 20}});
    CHECK(tri.value(1, 1) == 255);
    CHECK(tri.value(18, 0) == 255);
    CHECK(tri.value(18, 18) == 0);
    CHECK(tri.value(9, 9) == 255);    // centre (9.5, 9.5) is just inside
    CHECK(tri.value(10, 10) == 0);
  }
  SECTION("even-odd leaves the overlap of a bow tie's halves unselected") {
    // A square with a square hole, drawn as one outline that doubles back.
    const MaskGrid ring = polygon_mask({{0, 0}, {30, 0}, {30, 30}, {0, 30},
                                        {0, 0}, {10, 10}, {10, 20}, {20, 20},
                                        {20, 10}, {10, 10}});
    CHECK(ring.value(5, 5) == 255);
    CHECK(ring.value(15, 15) == 0);
    CHECK(ring.value(25, 25) == 255);
  }
  SECTION("too few points select nothing") {
    CHECK(polygon_mask({{0, 0}, {10, 10}}).empty());
  }
}

TEST_CASE("Flood selection", "[selection]") {
  // Left half black, right half white, with a grey square inside the black.
  Image pixels(40, 20, kBlack);
  pixels.fill({20, 0, 20, 20}, kWhite);
  pixels.fill({4, 4, 4, 4}, {20, 20, 20, 255});

  SECTION("tolerance 0 takes only exact matches") {
    const MaskGrid region = flood_mask(pixels, {0, 0}, {0, 0}, 0);
    CHECK(region.value(0, 0) == 255);
    CHECK(region.value(19, 19) == 255);
    CHECK(region.value(5, 5) == 0);   // the grey square is a hole
    CHECK(region.value(20, 0) == 0);  // white is not reached
    CHECK(count_at_least(region, {0, 0, 40, 20}, 255) == 400 - 16);
  }
  SECTION("enough tolerance swallows the grey square") {
    const MaskGrid region = flood_mask(pixels, {0, 0}, {0, 0}, 20);
    CHECK(region.value(5, 5) == 255);
    CHECK(region.value(20, 0) == 0);
  }
  SECTION("only contiguous pixels") {
    Image split(30, 10, kBlack);
    split.fill({10, 0, 10, 10}, kWhite);  // a white wall in the middle
    const MaskGrid region = flood_mask(split, {0, 0}, {0, 0}, 0);
    CHECK(region.value(9, 5) == 255);
    CHECK(region.value(25, 5) == 0);  // same colour, other side of the wall
  }
  SECTION("the image's document position is honoured") {
    const MaskGrid region = flood_mask(pixels, {100, 50}, {130, 55}, 0);
    CHECK(region.value(120, 50) == 255);
    CHECK(region.value(139, 69) == 255);
    CHECK(region.value(119, 50) == 0);
  }
  SECTION("a seed outside the image selects nothing") {
    CHECK(flood_mask(pixels, {0, 0}, {-1, 5}, 255).empty());
    CHECK(flood_mask(pixels, {0, 0}, {40, 5}, 255).empty());
  }
}

TEST_CASE("Combining selections", "[selection]") {
  const MaskGrid a = rect_mask({0, 0, 20, 10});
  const MaskGrid b = rect_mask({10, 0, 20, 10});

  CHECK(*combine_selection(a, b, SelectionOp::Replace) == b);
  CHECK(*combine_selection(std::nullopt, b, SelectionOp::Replace) == b);

  const MaskGrid added = *combine_selection(a, b, SelectionOp::Add);
  CHECK(added == rect_mask({0, 0, 30, 10}));
  CHECK(*combine_selection(std::nullopt, b, SelectionOp::Add) == b);

  const MaskGrid subtracted = *combine_selection(a, b, SelectionOp::Subtract);
  CHECK(subtracted == rect_mask({0, 0, 10, 10}));
  CHECK_FALSE(combine_selection(std::nullopt, b, SelectionOp::Subtract));

  CHECK(intersected(a, b) == rect_mask({10, 0, 10, 10}));
}

TEST_CASE("Inverting a selection", "[selection]") {
  const MaskGrid inside = rect_mask({10, 10, 5, 5});
  const MaskGrid outside = inverted(inside);
  CHECK(outside.background() == 255);
  CHECK(outside.value(12, 12) == 0);
  CHECK(outside.value(9, 12) == 255);
  CHECK(outside.value(-5000, 9000) == 255);  // everything beyond is selected
  CHECK(inverted(outside) == inside);
}

TEST_CASE("Feathering softens a selection's edge", "[selection]") {
  const MaskGrid hard = rect_mask({20, 20, 60, 60});
  const MaskGrid soft = feathered(hard, 9.0f);

  CHECK(soft.value(50, 50) == 255);  // deep inside stays selected
  CHECK(soft.value(0, 0) == 0);      // far outside stays unselected
  CHECK(soft.value(5, 50) == 0);
  // The edge between columns 19 and 20 becomes a ramp through the middle.
  CHECK(soft.value(19, 50) > 64);
  CHECK(soft.value(19, 50) < 192);
  CHECK(soft.value(20, 50) > 64);
  CHECK(soft.value(20, 50) < 192);
  for (int x = 10; x < 30; ++x) {
    CAPTURE(x);
    CHECK(soft.value(x, 50) <= soft.value(x + 1, 50));  // rising into the shape
  }
  // The same on every side.
  CHECK(soft.value(20, 50) == soft.value(79, 50));
  CHECK(soft.value(50, 20) == soft.value(50, 79));
  CHECK(soft.value(20, 50) == soft.value(50, 20));

  CHECK(feathered(hard, 0.0f) == hard);
  CHECK(feathered(MaskGrid(255), 5.0f) == MaskGrid(255));  // nothing to blur

  SECTION("an inverted selection feathers the same way") {
    const MaskGrid soft_outside = feathered(inverted(hard), 9.0f);
    CHECK(soft_outside.value(0, 0) == 255);
    CHECK(soft_outside.value(50, 50) == 0);
    CHECK(soft_outside.value(20, 50) > 64);
    CHECK(soft_outside.value(20, 50) < 192);
  }
}

TEST_CASE("Selection outline marks only the edge", "[selection]") {
  Image image(20, 20, {0, 255, 0, 255});
  draw_selection_outline(image, rect_mask({5, 5, 10, 10}), {0, 0});
  const Color green{0, 255, 0, 255};
  CHECK(image.pixel(4, 4) == green);    // outside
  CHECK(image.pixel(10, 10) == green);  // interior
  CHECK(image.pixel(5, 10) != green);   // left edge
  CHECK(image.pixel(14, 10) != green);  // right edge
  CHECK(image.pixel(10, 5) != green);   // top edge
  CHECK(image.pixel(10, 14) != green);  // bottom edge
  // Dashes: both colours appear along an edge.
  bool black = false, white = false;
  for (int x = 5; x < 15; ++x) {
    black |= image.pixel(x, 5) == kBlack;
    white |= image.pixel(x, 5) == kWhite;
  }
  CHECK(black);
  CHECK(white);

  SECTION("a selection running off the image has no edge at the border") {
    Image full(10, 10, green);
    draw_selection_outline(full, rect_mask({-5, -5, 30, 30}), {0, 0});
    CHECK(full == Image(10, 10, green));
  }
  SECTION("the image's document position is honoured") {
    Image shifted(20, 20, green);
    draw_selection_outline(shifted, rect_mask({105, 5, 10, 10}), {100, 0});
    CHECK(shifted.pixel(5, 10) != green);
    CHECK(shifted.pixel(10, 10) == green);
  }
}
