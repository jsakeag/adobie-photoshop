#include <cmath>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/canvas_commands.h"
#include "ps/selection.h"
#include "ps/transform_commands.h"
#include "state_checks.h"

using namespace ps;
using Catch::Approx;

namespace {

const Color kRed{255, 0, 0, 255};

Vec2 apply(const Matrix3& m, double x, double y) { return m.apply({x, y}); }

}  // namespace

TEST_CASE("Matrix3 basics", "[transform]") {
  const Vec2 p = apply(Matrix3{}, 3, 4);
  CHECK(p.x == 3);
  CHECK(p.y == 4);

  const Vec2 moved = apply(Matrix3::translate(10, -2), 3, 4);
  CHECK(moved.x == 13);
  CHECK(moved.y == 2);

  // Clockwise on screen: right turns into down.
  const Vec2 turned = apply(Matrix3::rotate(90), 1, 0);
  CHECK(turned.x == Approx(0).margin(1e-12));
  CHECK(turned.y == Approx(1));

  // Composition applies the right-hand matrix first.
  const Vec2 both = apply(Matrix3::translate(5, 0) * Matrix3::scale(2, 2), 1, 1);
  CHECK(both.x == 7);
  CHECK(both.y == 2);

  const Matrix3 m = Matrix3::translate(7, 3) * Matrix3::rotate(30) * Matrix3::scale(2, 0.5);
  const Vec2 back = m.inverse().apply(m.apply({11, -6}));
  CHECK(back.x == Approx(11));
  CHECK(back.y == Approx(-6));
  CHECK_THROWS_AS(Matrix3::scale(0, 1).inverse(), std::invalid_argument);

  const Vec2 centre = scale_rotate_about({10, 10}, 3, 3, 45).apply({10, 10});
  CHECK(centre.x == Approx(10));
  CHECK(centre.y == Approx(10));
}

TEST_CASE("Transformed bounds", "[transform]") {
  CHECK(transformed_bounds(Matrix3{}, {2, 3, 10, 5}) == Rect{2, 3, 10, 5});
  CHECK(transformed_bounds(Matrix3::scale(2, 3), {1, 1, 4, 4}) == Rect{2, 3, 8, 12});
  CHECK(transformed_bounds(Matrix3::rotate(90), {0, 0, 10, 4}) == Rect{-4, 0, 4, 10});
  CHECK(transformed_bounds(Matrix3::translate(0.5, 0), {0, 0, 2, 2}) == Rect{0, 0, 3, 2});
}

TEST_CASE("Resampling exact cases", "[transform]") {
  const Image pattern = make_test_pattern(20, 14);
  const Rect same{0, 0, 20, 14};
  for (Interpolation i : {Interpolation::Nearest, Interpolation::Bilinear,
                          Interpolation::Bicubic}) {
    CAPTURE(static_cast<int>(i));
    CHECK(transform_image(pattern, {0, 0}, Matrix3{}, same, i) == pattern);
    // Whole-pixel moves sample exactly at pixel centres.
    CHECK(transform_image(pattern, {0, 0}, Matrix3::translate(3, 2), {3, 2, 20, 14}, i) ==
          pattern);
    // So do quarter turns about the image's centre.
    const Matrix3 quarter = scale_rotate_about({10, 7}, 1, 1, 90);
    const Rect turned = transformed_bounds(quarter, same);
    CHECK(transform_image(pattern, {0, 0}, quarter, turned, i) ==
          pattern.rotated(Rotation::CW90));
  }
}

TEST_CASE("Resampling behaviour", "[transform]") {
  SECTION("nearest doubles pixels") {
    Image two(2, 1);
    two.set_pixel(0, 0, kRed);
    two.set_pixel(1, 0, kWhite);
    const Image big = transform_image(two, {0, 0}, Matrix3::scale(2, 2), {0, 0, 4, 2},
                                      Interpolation::Nearest);
    CHECK(big.pixel(1, 1) == kRed);
    CHECK(big.pixel(2, 0) == kWhite);
  }
  SECTION("a flat colour stays flat however it is scaled or turned") {
    const Image flat(16, 16, {40, 160, 90, 255});
    const Matrix3 m = scale_rotate_about({8, 8}, 1.7, 0.6, 33);
    const Image out = transform_image(flat, {0, 0}, m, {0, 0, 16, 16},
                                      Interpolation::Bicubic);
    CHECK(out.pixel(8, 8) == Color{40, 160, 90, 255});
    CHECK(out.pixel(3, 12) == Color{40, 160, 90, 255});  // edges repeat
  }
  SECTION("bilinear halfway between two pixels averages them") {
    Image two(2, 1);
    two.set_pixel(0, 0, kBlack);
    two.set_pixel(1, 0, kWhite);
    const Image mid = transform_image(two, {0, 0}, Matrix3::translate(0.5, 0),
                                      {1, 0, 1, 1}, Interpolation::Bilinear);
    CHECK(mid.pixel(0, 0) == Color{128, 128, 128, 255});
  }
  SECTION("bicubic keeps colours valid where it overshoots") {
    Image step(8, 1, kBlack);
    step.fill({4, 0, 4, 1}, kWhite);
    const Image out = transform_image(step, {0, 0}, Matrix3::scale(3.3, 1), {0, 0, 26, 1},
                                      Interpolation::Bicubic);
    for (int x = 0; x < 26; ++x) CHECK(out.pixel(x, 0).a == 255);
  }
}

TEST_CASE("Opaque and selected bounds", "[transform][selection]") {
  TileGrid grid;
  CHECK(opaque_bounds(grid).empty());
  grid.fill({5, 7, 3, 2}, kRed);
  grid.set_pixel(70, 1, {0, 0, 0, 1});
  CHECK(opaque_bounds(grid) == Rect{5, 1, 66, 8});

  const MaskGrid sel = rect_mask({10, 10, 5, 5});
  CHECK(selected_bounds(sel, {0, 0, 100, 100}) == Rect{10, 10, 5, 5});
  CHECK(selected_bounds(sel, {12, 0, 100, 100}) == Rect{12, 10, 3, 5});
  CHECK(selected_bounds(inverted(sel), {0, 0, 50, 40}) == Rect{0, 0, 50, 40});
  CHECK(selected_bounds(sel, {50, 50, 10, 10}).empty());
}

TEST_CASE("Image Size resamples every layer and mask", "[transform]") {
  Document doc(Image(40, 20, kWhite));
  Layer red;
  red.pixels.fill({0, 0, 10, 10}, kRed);
  red.offset = {20, 10};
  red.mask = MaskGrid(255);
  red.mask->fill({0, 0, 5, 10}, 0);
  doc.add_layer(1, red);
  doc.set_selection(rect_mask({0, 0, 5, 5}));

  ImageSizeCommand(80, 40, Interpolation::Nearest).execute(doc);
  CHECK(doc.width() == 80);
  CHECK(doc.height() == 40);
  CHECK_FALSE(doc.selection());
  CHECK(doc.layers().at(1).offset.x == 0);
  CHECK(doc.flatten().pixel(41, 21) == kWhite);  // masked half, doubled
  CHECK(doc.flatten().pixel(55, 25) == kRed);
  CHECK(doc.flatten().pixel(59, 39) == kRed);
  CHECK(doc.flatten().pixel(60, 25) == kWhite);
  CHECK(doc.flatten().pixel(79, 39) == kWhite);  // the background reaches the edge
  CHECK(cache_is_current(doc));

  SECTION("shrinking keeps the edges opaque") {
    ImageSizeCommand(30, 15, Interpolation::Bicubic).execute(doc);
    CHECK(doc.flatten().pixel(29, 14).a == 255);
    CHECK(doc.flatten().pixel(0, 0) == kWhite);
  }
  SECTION("an empty size is rejected") {
    ImageSizeCommand bad(0, 10, Interpolation::Bicubic);
    CHECK_THROWS_AS(bad.execute(doc), std::invalid_argument);
  }
}

TEST_CASE("Transforming a layer", "[transform]") {
  Document doc(100, 100);
  Layer red;
  red.pixels.fill({30, 40, 20, 10}, kRed);
  doc.add_layer(1, red);

  SECTION("half size about its centre") {
    const Matrix3 m = scale_rotate_about({40, 45}, 0.5, 0.5, 0);
    TransformLayerCommand(1, m, Interpolation::Bicubic).execute(doc);
    CHECK(opaque_bounds(doc.layers().at(1).pixels) == Rect{35, 42, 10, 6});
    CHECK(doc.flatten().pixel(40, 45) == kRed);
    CHECK(doc.flatten().pixel(32, 45) == kWhite);
  }
  SECTION("a quarter turn") {
    TransformLayerCommand(1, scale_rotate_about({40, 45}, 1, 1, 90),
                          Interpolation::Bicubic)
        .execute(doc);
    CHECK(opaque_bounds(doc.layers().at(1).pixels) == Rect{35, 35, 10, 20});
  }
  SECTION("a moved layer, with a mask that turns too") {
    doc.set_layer_offset(1, {-30, -40});  // content now at document (0, 0)
    MaskGrid mask(255);
    mask.fill({30, 40, 10, 10}, 0);  // hides the left half
    doc.set_layer_mask(1, mask);
    TransformLayerCommand(1, scale_rotate_about({10, 5}, 1, 1, 180),
                          Interpolation::Nearest)
        .execute(doc);
    CHECK(doc.flatten().pixel(2, 5) == kRed);     // the shown half moved left
    CHECK(doc.flatten().pixel(15, 5) == kWhite);  // the hidden half moved right
  }
  CHECK(cache_is_current(doc));
}

TEST_CASE("Crop to selection", "[transform][selection]") {
  Document doc(make_test_pattern(60, 40));
  const Image before = doc.flatten();
  CropToSelectionCommand crop;
  CHECK_THROWS_AS(crop.execute(doc), std::logic_error);

  doc.set_selection(ellipse_mask({10, 5, 20, 10}));
  CropToSelectionCommand().execute(doc);
  CHECK(doc.width() == 20);
  CHECK(doc.height() == 10);
  CHECK(doc.flatten() == before.cropped({10, 5, 20, 10}));
}

TEST_CASE("Transforms match their golden images", "[transform][golden]") {
  Document grown(make_test_pattern(40, 30));
  ImageSizeCommand(100, 75, Interpolation::Bicubic).execute(grown);
  CHECK(golden_mismatch(grown.flatten(), "transform_image_size") == "");

  Document turned(96, 96, {40, 40, 40, 255});
  Layer layer;
  layer.pixels.write(make_test_pattern(48, 32), {24, 32});
  turned.add_layer(1, layer);
  TransformLayerCommand(1, scale_rotate_about({48, 48}, 1.4, 1.0, 30),
                        Interpolation::Bicubic)
      .execute(turned);
  CHECK(golden_mismatch(turned.flatten(), "transform_scale_rotate") == "");
}
