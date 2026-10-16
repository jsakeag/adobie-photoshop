#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ps/brush.h"

using namespace ps;

namespace {

int covered(const MaskGrid& mask, Rect area, int threshold = 1) {
  int n = 0;
  for (std::uint8_t v : mask.read(area)) n += v >= threshold;
  return n;
}

BrushSettings hard(float diameter) {
  BrushSettings b;
  b.diameter = diameter;
  b.hardness = 1.0f;
  return b;
}

}  // namespace

TEST_CASE("A hard dab is a solid disc", "[brush]") {
  Stroke stroke(hard(9.0f));
  stroke.add_point({10.5, 10.5});  // centre of pixel (10, 10)
  const MaskGrid& c = stroke.coverage();
  CHECK(stroke.dab_count() == 1);
  CHECK(c.value(10, 10) == 255);
  CHECK(c.value(14, 10) == 255);  // 4 px out, inside the 4.5 radius
  CHECK(c.value(15, 10) == 0);
  CHECK(c.value(13, 13) == 255);  // 4.24 px out
  CHECK(c.value(14, 13) == 0);    // 5 px out
  // Lattice points within radius 4.5 of the centre.
  CHECK(covered(c, {0, 0, 30, 30}) == 69);
  CHECK(covered(c, {0, 0, 30, 30}, 255) == 69);
}

TEST_CASE("A soft dab fades toward its rim", "[brush]") {
  BrushSettings soft;
  soft.diameter = 21.0f;
  soft.hardness = 0.0f;
  Stroke stroke(soft);
  stroke.add_point({10.5, 10.5});
  const MaskGrid& c = stroke.coverage();
  CHECK(c.value(10, 10) > 250);
  for (int x = 10; x < 20; ++x) {
    CAPTURE(x);
    CHECK(c.value(x, 10) >= c.value(x + 1, 10));
  }
  CHECK(c.value(15, 10) > 50);
  CHECK(c.value(15, 10) < 205);
  CHECK(c.value(20, 10) < 10);
  CHECK(c.value(21, 10) == 0);
}

TEST_CASE("Flow builds coverage up dab by dab", "[brush]") {
  BrushSettings light = hard(9.0f);
  light.flow = 0.5f;
  Stroke stroke(light);
  stroke.add_point({10.5, 10.5});
  CHECK(stroke.coverage().value(10, 10) == 128);

  Stroke twice(light);
  twice.add_point({10.5, 10.5});
  twice.add_point({10.5, 10.5});  // no distance: no new dab
  CHECK(twice.dab_count() == 1);

  // Dabs one pixel apart overlap and keep adding, never past full.
  light.spacing = 0.0f;  // the minimum step is one pixel
  Stroke dense(light);
  dense.add_point({10.5, 10.5});
  dense.add_point({13.5, 10.5});
  CHECK(dense.dab_count() == 4);
  CHECK(dense.coverage().value(12, 10) > 128);
  CHECK(dense.coverage().value(12, 10) < 255);
}

TEST_CASE("Dabs are evenly spaced along the stroke", "[brush]") {
  BrushSettings b = hard(20.0f);
  b.spacing = 0.25f;  // a dab every 5 px

  Stroke pieces(b);
  pieces.add_point({0, 0});
  CHECK(pieces.dab_count() == 1);
  pieces.add_point({23, 0});  // dabs at 5, 10, 15, 20
  CHECK(pieces.dab_count() == 5);
  pieces.add_point({30, 0});  // spacing carries over: dabs at 25, 30
  CHECK(pieces.dab_count() == 7);

  Stroke whole(b);
  whole.add_point({0, 0});
  whole.add_point({30, 0});
  CHECK(whole.dab_count() == 7);
  CHECK(whole.coverage() == pieces.coverage());

  SECTION("diagonal strokes measure true distance") {
    Stroke diagonal(b);
    diagonal.add_point({0, 0});
    diagonal.add_point({30, 40});  // 50 px long
    CHECK(diagonal.dab_count() == 11);
  }
}
