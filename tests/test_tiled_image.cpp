#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/tiled_image.h"

using namespace ps;

namespace {

constexpr int kT = TiledImage::kTileSize;

}  // namespace

TEST_CASE("TiledImage construction", "[tiled]") {
  const TiledImage empty;
  CHECK(empty.empty());
  CHECK(empty.tile_columns() == 0);
  CHECK(empty.flatten().empty());

  const TiledImage image(kT + 44, 2 * kT, kWhite);
  CHECK(image.width() == kT + 44);
  CHECK(image.height() == 2 * kT);
  CHECK(image.tile_columns() == 2);
  CHECK(image.tile_rows() == 2);
  CHECK(image.pixel(0, 0) == kWhite);
  CHECK(image.pixel(kT + 43, 2 * kT - 1) == kWhite);

  CHECK(TiledImage(1, 1).tile_columns() == 1);
  CHECK(TiledImage(kT, kT).tile_columns() == 1);
  CHECK(TiledImage(kT + 1, kT).tile_columns() == 2);
  CHECK_THROWS_AS(TiledImage(-1, 4), std::invalid_argument);
}

TEST_CASE("TiledImage pixel access is bounds-checked", "[tiled]") {
  TiledImage image(kT + 10, kT + 10);
  image.set_pixel(kT + 3, kT + 4, kWhite);
  CHECK(image.pixel(kT + 3, kT + 4) == kWhite);
  CHECK(image.pixel(kT + 4, kT + 4) == kTransparent);
  CHECK(image.pixel(3, 4) == kTransparent);
  CHECK_THROWS_AS(image.pixel(kT + 10, 0), std::out_of_range);
  CHECK_THROWS_AS(image.pixel(0, -1), std::out_of_range);
  CHECK_THROWS_AS(image.set_pixel(-1, 0, kWhite), std::out_of_range);
}

TEST_CASE("TiledImage only allocates tiles that are written", "[tiled]") {
  TiledImage image(3 * kT, 2 * kT, kWhite);
  CHECK(image.allocated_tiles() == 0);

  image.set_pixel(kT + 5, kT + 5, kBlack);
  CHECK(image.allocated_tiles() == 1);
  CHECK(image.pixel(kT + 5, kT + 5) == kBlack);
  CHECK(image.pixel(kT + 6, kT + 5) == kWhite);  // rest of the tile keeps the fill

  SECTION("a fill that covers a whole tile releases its storage") {
    image.fill({kT, kT, kT, kT}, kBlack);
    CHECK(image.allocated_tiles() == 0);
    CHECK(image.pixel(kT + 6, kT + 5) == kBlack);
  }

  SECTION("a partial fill allocates only the tiles it partly covers") {
    // Covers tile (0,0) fully and a strip of tiles (1,0), (0,1) and (1,1).
    image.fill({0, 0, kT + 10, kT + 10}, kBlack);
    CHECK(image.allocated_tiles() == 3);
    CHECK(image.pixel(kT + 9, kT + 9) == kBlack);
    CHECK(image.pixel(kT + 10, kT + 9) == kWhite);
    CHECK(image.pixel(kT + 9, kT + 10) == kWhite);
    CHECK(image.pixel(0, 0) == kBlack);
  }

  SECTION("a fill clipped by the image edge still counts as the whole tile") {
    TiledImage ragged(kT + 20, kT + 20);
    ragged.set_pixel(kT + 1, kT + 1, kWhite);
    ragged.fill({kT, kT, 5000, 5000}, kBlack);
    CHECK(ragged.allocated_tiles() == 0);
    CHECK(ragged.pixel(kT + 19, kT + 19) == kBlack);
  }
}

TEST_CASE("TiledImage round-trips a flat image", "[tiled]") {
  const Image pattern = make_test_pattern(2 * kT + 77, kT + 33);
  const TiledImage tiled(pattern);
  CHECK(tiled.tile_columns() == 3);
  CHECK(tiled.tile_rows() == 2);
  CHECK(tiled.flatten() == pattern);
  CHECK(tiled.pixel(kT, kT) == pattern.pixel(kT, kT));
  CHECK(tiled.pixel(2 * kT + 76, kT + 32) == pattern.pixel(2 * kT + 76, kT + 32));
}

TEST_CASE("TiledImage read matches Image::cropped", "[tiled]") {
  const Image pattern = make_test_pattern(2 * kT + 77, kT + 33);
  const TiledImage tiled(pattern);

  const Rect rects[] = {
      {kT - 5, kT - 5, 10, 10},          // straddles four tiles
      {10, 20, 30, 40},                  // inside one tile
      {kT, 0, kT, kT},                   // exactly one tile
      {-20, -30, 100, 100},              // hangs off the top-left
      {2 * kT + 50, kT, 100, 100},       // hangs off the bottom-right
      {-10, -10, 2 * kT + 100, kT + 60}, // larger than the image
      {5000, 5000, 8, 8},                // misses entirely
      {3, 3, 0, 0},                      // empty
  };
  for (const Rect& r : rects) {
    CAPTURE(r.x, r.y, r.width, r.height);
    CHECK(tiled.read(r, kWhite) == pattern.cropped(r, kWhite));
  }

  CHECK_THROWS_AS(tiled.read({0, 0, -1, 1}), std::invalid_argument);
}

TEST_CASE("TiledImage read sees solid tiles", "[tiled]") {
  TiledImage image(2 * kT, kT, kWhite);
  image.set_pixel(kT - 1, 0, kBlack);  // allocates the left tile only
  const Image strip = image.read({kT - 2, 0, 4, 1});
  CHECK(strip.pixel(0, 0) == kWhite);
  CHECK(strip.pixel(1, 0) == kBlack);
  CHECK(strip.pixel(2, 0) == kWhite);  // from the solid right tile
  CHECK(strip.pixel(3, 0) == kWhite);
}

TEST_CASE("TiledImage write matches Image::blit", "[tiled]") {
  const Image stamp = make_test_pattern(kT + 40, 90);
  const Point positions[] = {
      {kT - 20, kT - 45},  // straddles four tiles
      {5, 5},
      {-30, -30},          // clipped at the top-left
      {2 * kT - 10, kT + 10},  // clipped at the bottom-right
      {9000, 0},           // misses entirely
  };
  for (const Point& p : positions) {
    CAPTURE(p.x, p.y);
    Image flat(2 * kT + 30, kT + 60, kWhite);
    TiledImage tiled(2 * kT + 30, kT + 60, kWhite);
    flat.blit(stamp, p);
    tiled.write(stamp, p);
    CHECK(tiled.flatten() == flat);
  }
}

TEST_CASE("TiledImage copies share tiles until written", "[tiled]") {
  const Image pattern = make_test_pattern(2 * kT, 2 * kT);
  TiledImage original(pattern);
  CHECK(original.allocated_tiles() == 4);

  TiledImage copy = original;
  CHECK(copy.shared_tiles(original) == 4);

  copy.set_pixel(kT + 1, 1, kBlack);  // top-right tile
  CHECK(copy.shared_tiles(original) == 3);
  CHECK(copy.pixel(kT + 1, 1) == kBlack);
  CHECK(original.flatten() == pattern);  // the original is untouched

  // Writing to the original must not leak into the copy either.
  original.fill({0, kT, 10, 10}, kWhite);  // bottom-left tile
  CHECK(copy.shared_tiles(original) == 2);
  CHECK(copy.pixel(0, kT) == pattern.pixel(0, kT));
  CHECK(original.pixel(0, kT) == kWhite);

  // A tile this image now owns alone is written in place, not cloned again.
  copy.set_pixel(kT + 2, 1, kBlack);
  CHECK(copy.pixel(kT + 1, 1) == kBlack);
  CHECK(copy.shared_tiles(original) == 2);
}

TEST_CASE("TiledImage equality compares content, not storage", "[tiled]") {
  TiledImage solid(kT + 10, kT, kWhite);
  TiledImage stored(kT + 10, kT, kBlack);
  stored.write(Image(kT + 10, kT, kWhite), {0, 0});
  CHECK(solid.allocated_tiles() == 0);
  CHECK(stored.allocated_tiles() == 2);
  CHECK(solid == stored);
  CHECK(stored == solid);

  stored.set_pixel(kT + 9, kT - 1, kBlack);
  CHECK(solid != stored);

  CHECK(TiledImage(4, 4) != TiledImage(4, 5));
  CHECK(TiledImage(4, 4, kWhite) != TiledImage(4, 4, kBlack));
  CHECK(TiledImage(4, 4, kWhite) == TiledImage(4, 4, kWhite));

  const TiledImage copy = stored;
  CHECK(copy == stored);
}
