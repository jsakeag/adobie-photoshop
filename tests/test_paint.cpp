#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/brush.h"
#include "ps/paint.h"
#include "ps/selection.h"
#include "state_checks.h"

using namespace ps;

namespace {

const Color kRed{255, 0, 0, 255};

}  // namespace

TEST_CASE("Painting composites a colour through coverage", "[paint]") {
  Document doc(64, 64);
  const MaskGrid box = rect_mask({10, 10, 10, 10});

  SECTION("full strength") {
    paint_coverage(doc, 0, PaintTarget::Pixels, box, kRed, 1.0f, false);
    CHECK(doc.flatten().pixel(10, 10) == kRed);
    CHECK(doc.flatten().pixel(19, 19) == kRed);
    CHECK(doc.flatten().pixel(20, 19) == kWhite);
  }
  SECTION("opacity") {
    paint_coverage(doc, 0, PaintTarget::Pixels, box, kRed, 0.5f, false);
    CHECK(doc.flatten().pixel(15, 15) == Color{255, 128, 128, 255});
  }
  SECTION("partial coverage") {
    MaskGrid half(0);
    half.fill({10, 10, 10, 10}, 128);
    paint_coverage(doc, 0, PaintTarget::Pixels, half, kBlack, 1.0f, false);
    CHECK(doc.flatten().pixel(15, 15) == Color{127, 127, 127, 255});
  }
  SECTION("limited to the selection") {
    doc.set_selection(rect_mask({15, 0, 64, 64}));
    paint_coverage(doc, 0, PaintTarget::Pixels, box, kRed, 1.0f, false);
    CHECK(doc.flatten().pixel(14, 15) == kWhite);
    CHECK(doc.flatten().pixel(15, 15) == kRed);
  }
  SECTION("a feathered selection paints partly at its edge") {
    MaskGrid soft(0);
    soft.fill({0, 0, 64, 64}, 128);
    doc.set_selection(soft);
    paint_coverage(doc, 0, PaintTarget::Pixels, box, kBlack, 1.0f, false);
    CHECK(doc.flatten().pixel(15, 15) == Color{127, 127, 127, 255});
  }
  SECTION("limited to the canvas") {
    paint_coverage(doc, 0, PaintTarget::Pixels, rect_mask({-50, -50, 300, 300}),
                   kRed, 1.0f, false);
    CHECK(doc.flatten() == Image(64, 64, kRed));
    CHECK(doc.layers().at(0).pixels.pixel(-1, -1) == kTransparent);
    CHECK(doc.layers().at(0).pixels.pixel(64, 0) == kTransparent);
  }
  SECTION("on a moved layer, in document coordinates") {
    Layer top;
    top.offset = {5, 5};
    doc.add_layer(1, top);
    paint_coverage(doc, 1, PaintTarget::Pixels, box, kRed, 1.0f, false);
    CHECK(doc.layers().at(1).pixels.pixel(5, 5) == kRed);  // document (10, 10)
    CHECK(doc.layers().at(1).pixels.pixel(4, 5) == kTransparent);
    CHECK(doc.flatten().pixel(10, 10) == kRed);
  }
  CHECK(cache_is_current(doc));
}

TEST_CASE("Erasing lowers alpha", "[paint]") {
  Document doc(Image(32, 32, kRed));
  const MaskGrid box = rect_mask({0, 0, 10, 10});

  paint_coverage(doc, 0, PaintTarget::Pixels, box, kBlack, 0.5f, true);
  CHECK(doc.layers().at(0).pixels.pixel(5, 5) == Color{255, 0, 0, 128});
  paint_coverage(doc, 0, PaintTarget::Pixels, box, kBlack, 1.0f, true);
  CHECK(doc.layers().at(0).pixels.pixel(5, 5) == kTransparent);
  CHECK(doc.layers().at(0).pixels.pixel(15, 5) == kRed);
  CHECK(cache_is_current(doc));
}

TEST_CASE("Painting on a layer mask", "[paint][mask]") {
  Document doc(32, 32);
  Layer red;
  red.pixels.fill({0, 0, 32, 32}, kRed);
  red.mask = MaskGrid(255);
  doc.add_layer(1, red);
  const MaskGrid box = rect_mask({0, 0, 10, 10});

  paint_coverage(doc, 1, PaintTarget::Mask, box, kBlack, 1.0f, false);
  CHECK(doc.layers().at(1).mask->value(5, 5) == 0);
  CHECK(doc.flatten().pixel(5, 5) == kWhite);  // hidden: background shows
  CHECK(doc.flatten().pixel(15, 5) == kRed);

  paint_coverage(doc, 1, PaintTarget::Mask, box, kBlack, 0.5f, true);
  CHECK(doc.layers().at(1).mask->value(5, 5) == 128);  // halfway back to white
  paint_coverage(doc, 1, PaintTarget::Mask, box, kBlack, 1.0f, true);
  CHECK(doc.layers().at(1).mask->value(5, 5) == 255);
  CHECK(doc.flatten().pixel(5, 5) == kRed);

  paint_coverage(doc, 1, PaintTarget::Mask, box, {128, 128, 128, 255}, 1.0f, false);
  CHECK(doc.layers().at(1).mask->value(5, 5) == 128);
  CHECK(cache_is_current(doc));

  CHECK_THROWS_AS(
      paint_coverage(doc, 0, PaintTarget::Mask, box, kBlack, 1.0f, false),
      std::logic_error);
}

TEST_CASE("Grey level of a colour", "[paint]") {
  CHECK(grey_level(kBlack) == 0);
  CHECK(grey_level(kWhite) == 255);
  CHECK(grey_level(kRed) == 76);
  CHECK(grey_level({0, 255, 0, 255}) == 150);
}

TEST_CASE("Brush strokes and a feathered fill match their golden image",
          "[paint][golden]") {
  Document doc(160, 100);
  const auto stroke = [&](BrushSettings b, Color c, double y) {
    Stroke s(b);
    for (double x = 15; x <= 145; x += 5) s.add_point({x, y + 6 * ((int(x) / 5) % 2)});
    paint_coverage(doc, 0, PaintTarget::Pixels, s.coverage(), c, b.opacity, false);
  };
  BrushSettings hard_brush;
  hard_brush.diameter = 9;
  hard_brush.hardness = 1.0f;
  stroke(hard_brush, kBlack, 15);

  BrushSettings soft_brush;
  soft_brush.diameter = 15;
  soft_brush.hardness = 0.0f;
  stroke(soft_brush, kRed, 40);

  BrushSettings airbrush;
  airbrush.diameter = 13;
  airbrush.hardness = 0.5f;
  airbrush.flow = 0.3f;
  airbrush.spacing = 0.1f;
  airbrush.opacity = 0.7f;
  stroke(airbrush, {0, 0, 255, 255}, 65);

  doc.set_selection(feathered(ellipse_mask({110, 60, 40, 30}), 6.0f));
  paint_coverage(doc, 0, PaintTarget::Pixels, rect_mask(doc.bounds()),
                 {0, 160, 0, 255}, 1.0f, false);

  CHECK(golden_mismatch(doc.flatten(), "paint_strokes") == "");
}
