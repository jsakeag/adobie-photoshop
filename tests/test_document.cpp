#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/document.h"

using namespace ps;

TEST_CASE("Document construction", "[document]") {
  const Document doc(8, 6);
  CHECK(doc.width() == 8);
  CHECK(doc.height() == 6);
  CHECK(doc.pixels().pixel(7, 5) == kWhite);

  CHECK_THROWS_AS(Document(0, 5), std::invalid_argument);
  CHECK_THROWS_AS(Document(Image()), std::invalid_argument);
}

TEST_CASE("Canvas resize places content by anchor", "[document]") {
  const Color red{255, 0, 0, 255};

  SECTION("growing from the top-left keeps content at the origin") {
    Document doc(2, 2, red);
    doc.resize_canvas(6, 4, Anchor::TopLeft, kBlack);
    CHECK(doc.width() == 6);
    CHECK(doc.height() == 4);
    CHECK(doc.pixels().pixel(0, 0) == red);
    CHECK(doc.pixels().pixel(1, 1) == red);
    CHECK(doc.pixels().pixel(2, 1) == kBlack);
    CHECK(doc.pixels().pixel(1, 2) == kBlack);
  }

  SECTION("growing from the centre pads evenly") {
    Document doc(2, 2, red);
    doc.resize_canvas(6, 4, Anchor::Center, kBlack);
    CHECK(doc.pixels().pixel(1, 1) == kBlack);
    CHECK(doc.pixels().pixel(2, 1) == red);
    CHECK(doc.pixels().pixel(3, 2) == red);
    CHECK(doc.pixels().pixel(4, 2) == kBlack);
    CHECK(doc.pixels().pixel(2, 0) == kBlack);
    CHECK(doc.pixels().pixel(2, 3) == kBlack);
  }

  SECTION("growing from the bottom-right pushes content to that corner") {
    Document doc(2, 2, red);
    doc.resize_canvas(6, 4, Anchor::BottomRight, kBlack);
    CHECK(doc.pixels().pixel(4, 2) == red);
    CHECK(doc.pixels().pixel(5, 3) == red);
    CHECK(doc.pixels().pixel(3, 3) == kBlack);
    CHECK(doc.pixels().pixel(5, 1) == kBlack);
  }

  SECTION("shrinking trims away from the anchor") {
    Document doc(make_test_pattern(16, 16));
    const Image before = doc.pixels();
    doc.resize_canvas(8, 8, Anchor::Right);
    CHECK(doc.pixels() == before.cropped({8, 4, 8, 8}));
  }

  SECTION("rejects an empty canvas") {
    Document doc(2, 2);
    CHECK_THROWS_AS(doc.resize_canvas(0, 4), std::invalid_argument);
  }
}

TEST_CASE("Document crop", "[document]") {
  Document doc(make_test_pattern(16, 16));
  const Image before = doc.pixels();

  doc.crop({12, 10, 100, 100});  // clipped to the canvas
  CHECK(doc.width() == 4);
  CHECK(doc.height() == 6);
  CHECK(doc.pixels() == before.cropped({12, 10, 4, 6}));

  CHECK_THROWS_AS(doc.crop({50, 50, 4, 4}), std::invalid_argument);
  CHECK(doc.width() == 4);  // unchanged after a failed crop
}

TEST_CASE("Document rotate and flip", "[document]") {
  Document doc(make_test_pattern(12, 8));
  const Image before = doc.pixels();

  doc.rotate(Rotation::CW90);
  CHECK(doc.width() == 8);
  CHECK(doc.height() == 12);
  doc.rotate(Rotation::CCW90);
  CHECK(doc.pixels() == before);

  doc.flip_horizontal();
  doc.flip_vertical();
  CHECK(doc.pixels() == before.rotated(Rotation::R180));
}

TEST_CASE("Canvas operations match golden images", "[document][golden]") {
  SECTION("test pattern") {
    CHECK(golden_mismatch(make_test_pattern(64, 48), "test_pattern") == "");
  }

  SECTION("rotate then grow the canvas") {
    Document doc(make_test_pattern(64, 48));
    doc.rotate(Rotation::CW90);
    doc.resize_canvas(80, 80, Anchor::Center, {40, 40, 40, 255});
    CHECK(golden_mismatch(doc.pixels(), "canvas_rotate_grow") == "");
  }

  SECTION("crop then flip") {
    Document doc(make_test_pattern(64, 48));
    doc.crop({0, 16, 40, 32});
    doc.flip_horizontal();
    CHECK(golden_mismatch(doc.pixels(), "canvas_crop_flip") == "");
  }
}
