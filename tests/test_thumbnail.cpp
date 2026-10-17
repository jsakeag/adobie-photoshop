#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/thumbnail.h"

using namespace ps;

namespace {
const Color kRed{255, 0, 0, 255};
}  // namespace

TEST_CASE("Layer thumbnails", "[thumbnail]") {
  Document doc(400, 200);
  Layer red;
  red.pixels.fill({0, 0, 100, 100}, kRed);
  red.offset = {200, 0};  // the right half's top-left quarter
  doc.add_layer(1, red);

  const Image thumb = layer_thumbnail(doc, 1, 40);
  CHECK(thumb.width() == 40);  // the canvas's shape, fitted
  CHECK(thumb.height() == 20);
  CHECK(thumb.pixel(25, 5) == kRed);
  CHECK(thumb.pixel(5, 5) == kTransparent);
  CHECK(thumb.pixel(25, 15) == kTransparent);

  CHECK(layer_thumbnail(doc, 0, 40) == Image(40, 20, kWhite));

  SECTION("the mask shows; opacity and visibility do not") {
    doc.set_layer_opacity(1, 0.1f);
    doc.set_layer_visible(1, false);
    MaskGrid mask(255);
    mask.fill({0, 0, 50, 100}, 0);
    doc.set_layer_mask(1, mask);
    const Image masked = layer_thumbnail(doc, 1, 40);
    CHECK(masked.pixel(21, 5) == kTransparent);  // hidden by the mask
    CHECK(masked.pixel(27, 5) == kRed);
  }
  SECTION("small documents are not enlarged") {
    const Document tiny(10, 5);
    const Image t = layer_thumbnail(tiny, 0, 40);
    CHECK(t.width() == 10);
    CHECK(t.height() == 5);
  }
  SECTION("an empty layer gives a transparent thumbnail") {
    doc.add_layer(2, Layer{});
    CHECK(layer_thumbnail(doc, 2, 40) == Image(40, 20));
  }
}
