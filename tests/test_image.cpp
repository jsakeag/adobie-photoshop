#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "ps/image.h"

using namespace ps;

namespace {

// 3x2 image where every pixel is unique: r = x, g = y.
Image numbered() {
  Image image(3, 2);
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 3; ++x) {
      image.set_pixel(x, y, {static_cast<std::uint8_t>(x),
                             static_cast<std::uint8_t>(y), 0, 255});
    }
  }
  return image;
}

}  // namespace

TEST_CASE("Rect intersection", "[geometry]") {
  const Rect a{0, 0, 10, 10};
  CHECK(a.intersect({5, 5, 10, 10}) == Rect{5, 5, 5, 5});
  CHECK(a.intersect({-5, -5, 8, 8}) == Rect{0, 0, 3, 3});
  CHECK(a.intersect({10, 0, 5, 5}).empty());
  CHECK(a.intersect({20, 20, 5, 5}).empty());
}

TEST_CASE("Image construction", "[image]") {
  const Image empty;
  CHECK(empty.empty());
  CHECK(empty.size_bytes() == 0);

  const Image image(4, 3, {10, 20, 30, 40});
  CHECK(image.width() == 4);
  CHECK(image.height() == 3);
  CHECK(image.size_bytes() == 4 * 3 * 4);
  CHECK(image.pixel(0, 0) == Color{10, 20, 30, 40});
  CHECK(image.pixel(3, 2) == Color{10, 20, 30, 40});

  CHECK(Image(2, 2).pixel(1, 1) == kTransparent);
  CHECK_THROWS_AS(Image(-1, 2), std::invalid_argument);
}

TEST_CASE("Image pixel access is bounds-checked", "[image]") {
  Image image(2, 2);
  image.set_pixel(1, 0, kWhite);
  CHECK(image.pixel(1, 0) == kWhite);
  CHECK(image.pixel(0, 1) == kTransparent);
  CHECK_THROWS_AS(image.pixel(2, 0), std::out_of_range);
  CHECK_THROWS_AS(image.pixel(0, -1), std::out_of_range);
  CHECK_THROWS_AS(image.set_pixel(0, 2, kWhite), std::out_of_range);
}

TEST_CASE("Image fill clips its rectangle", "[image]") {
  Image image(4, 4, kBlack);
  image.fill({2, 2, 10, 10}, kWhite);
  CHECK(image.pixel(1, 1) == kBlack);
  CHECK(image.pixel(2, 1) == kBlack);
  CHECK(image.pixel(2, 2) == kWhite);
  CHECK(image.pixel(3, 3) == kWhite);

  image.fill({-5, -5, 2, 2}, kWhite);  // entirely outside, no effect
  CHECK(image.pixel(0, 0) == kBlack);
}

TEST_CASE("Image blit clips on every side", "[image]") {
  const Image src = numbered();

  Image inside(5, 5, kWhite);
  inside.blit(src, {1, 2});
  CHECK(inside.pixel(0, 2) == kWhite);
  CHECK(inside.pixel(1, 2) == src.pixel(0, 0));
  CHECK(inside.pixel(3, 3) == src.pixel(2, 1));
  CHECK(inside.pixel(4, 3) == kWhite);

  Image top_left(5, 5, kWhite);
  top_left.blit(src, {-2, -1});
  CHECK(top_left.pixel(0, 0) == src.pixel(2, 1));
  CHECK(top_left.pixel(1, 0) == kWhite);

  Image bottom_right(5, 5, kWhite);
  bottom_right.blit(src, {4, 4});
  CHECK(bottom_right.pixel(4, 4) == src.pixel(0, 0));
  CHECK(bottom_right.pixel(3, 4) == kWhite);

  Image missed(5, 5, kWhite);
  missed.blit(src, {5, 0});
  CHECK(missed == Image(5, 5, kWhite));
}

TEST_CASE("Image cropped", "[image]") {
  const Image src = numbered();

  const Image inner = src.cropped({1, 0, 2, 2});
  CHECK(inner.width() == 2);
  CHECK(inner.height() == 2);
  CHECK(inner.pixel(0, 0) == src.pixel(1, 0));
  CHECK(inner.pixel(1, 1) == src.pixel(2, 1));

  const Image padded = src.cropped({-1, -1, 5, 4}, kWhite);
  CHECK(padded.pixel(0, 0) == kWhite);
  CHECK(padded.pixel(1, 1) == src.pixel(0, 0));
  CHECK(padded.pixel(3, 2) == src.pixel(2, 1));
  CHECK(padded.pixel(4, 3) == kWhite);

  CHECK_THROWS_AS(src.cropped({0, 0, -1, 1}), std::invalid_argument);
}

TEST_CASE("Image flips", "[image]") {
  const Image src = numbered();

  const Image h = src.flipped_horizontal();
  CHECK(h.pixel(0, 0) == src.pixel(2, 0));
  CHECK(h.pixel(2, 1) == src.pixel(0, 1));
  CHECK(h.flipped_horizontal() == src);

  const Image v = src.flipped_vertical();
  CHECK(v.pixel(0, 0) == src.pixel(0, 1));
  CHECK(v.pixel(2, 1) == src.pixel(2, 0));
  CHECK(v.flipped_vertical() == src);
}

TEST_CASE("Image rotation", "[image]") {
  const Image src = numbered();

  const Image cw = src.rotated(Rotation::CW90);
  CHECK(cw.width() == 2);
  CHECK(cw.height() == 3);
  // The top-left pixel moves to the top-right corner.
  CHECK(cw.pixel(1, 0) == src.pixel(0, 0));
  CHECK(cw.pixel(1, 2) == src.pixel(2, 0));
  CHECK(cw.pixel(0, 0) == src.pixel(0, 1));

  const Image ccw = src.rotated(Rotation::CCW90);
  // The top-left pixel moves to the bottom-left corner.
  CHECK(ccw.pixel(0, 2) == src.pixel(0, 0));
  CHECK(ccw.pixel(0, 0) == src.pixel(2, 0));

  CHECK(cw.rotated(Rotation::CCW90) == src);
  CHECK(cw.rotated(Rotation::CW90) == src.rotated(Rotation::R180));
  CHECK(src.rotated(Rotation::R180) ==
        src.flipped_horizontal().flipped_vertical());
}
