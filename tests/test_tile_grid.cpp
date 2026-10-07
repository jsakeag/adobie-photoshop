#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/tile_grid.h"

using namespace ps;

namespace {

constexpr int kT = TileGrid::kTileSize;
const Color kRed{255, 0, 0, 255};

}  // namespace

TEST_CASE("An empty TileGrid is transparent everywhere", "[tiles]") {
  const TileGrid grid;
  CHECK(grid.empty());
  CHECK(grid.tile_count() == 0);
  CHECK(grid.bounds().empty());
  CHECK(grid.pixel(0, 0) == kTransparent);
  CHECK(grid.pixel(-5000, 12345) == kTransparent);
  CHECK(grid.read({-10, -10, 40, 30}) == Image(40, 30));
  CHECK(grid.read({0, 0, 0, 0}).empty());
  CHECK_THROWS_AS(grid.read({0, 0, -1, 1}), std::invalid_argument);
}

TEST_CASE("TileGrid allocates tiles lazily", "[tiles]") {
  TileGrid grid;
  grid.set_pixel(5, 6, kRed);
  CHECK(grid.tile_count() == 1);
  CHECK(grid.pixel(5, 6) == kRed);
  CHECK(grid.pixel(6, 6) == kTransparent);
  CHECK(grid.bounds() == Rect{0, 0, kT, kT});

  grid.set_pixel(7, 8, kWhite);  // same tile
  CHECK(grid.tile_count() == 1);

  grid.set_pixel(3 * kT, 0, kWhite);
  CHECK(grid.tile_count() == 2);
  CHECK(grid.bounds() == Rect{0, 0, 4 * kT, kT});

  grid.clear();
  CHECK(grid.empty());
  CHECK(grid.pixel(5, 6) == kTransparent);
}

TEST_CASE("TileGrid handles negative coordinates", "[tiles]") {
  TileGrid grid;
  grid.set_pixel(-1, -1, kRed);
  grid.set_pixel(-kT, -kT, kWhite);  // far corner of the same tile
  CHECK(grid.tile_count() == 1);
  CHECK(grid.bounds() == Rect{-kT, -kT, kT, kT});
  CHECK(grid.pixel(-1, -1) == kRed);
  CHECK(grid.pixel(-kT, -kT) == kWhite);
  CHECK(grid.pixel(0, 0) == kTransparent);

  grid.set_pixel(-kT - 1, 0, kBlack);  // next tile to the left, one row down
  CHECK(grid.tile_count() == 2);
  CHECK(grid.pixel(-kT - 1, 0) == kBlack);
  CHECK(grid.bounds() == Rect{-2 * kT, -kT, 2 * kT, 2 * kT});
}

TEST_CASE("TileGrid fill", "[tiles]") {
  TileGrid grid;

  SECTION("covers exactly the rectangle") {
    grid.fill({-10, -10, kT + 20, 20}, kRed);
    CHECK(grid.pixel(-10, -10) == kRed);
    CHECK(grid.pixel(kT + 9, 9) == kRed);
    CHECK(grid.pixel(-11, -10) == kTransparent);
    CHECK(grid.pixel(kT + 10, 9) == kTransparent);
    CHECK(grid.pixel(0, 10) == kTransparent);
    CHECK(grid.tile_count() == 6);  // 3 columns x 2 rows, all partly covered
  }

  SECTION("fully covered tiles stay independent after a later write") {
    grid.fill({0, 0, 3 * kT, 2 * kT}, kRed);
    CHECK(grid.tile_count() == 6);
    grid.set_pixel(kT + 1, 1, kBlack);
    CHECK(grid.pixel(kT + 1, 1) == kBlack);
    CHECK(grid.pixel(1, 1) == kRed);
    CHECK(grid.pixel(2 * kT + 1, kT + 1) == kRed);
    Image expected(3 * kT, 2 * kT, kRed);
    expected.set_pixel(kT + 1, 1, kBlack);
    CHECK(grid.read({0, 0, 3 * kT, 2 * kT}) == expected);
  }

  SECTION("transparent releases covered tiles and allocates nothing") {
    grid.fill({0, 0, 2 * kT, 2 * kT}, kRed);
    grid.fill({0, 0, kT + 10, kT}, kTransparent);
    CHECK(grid.tile_count() == 3);
    CHECK(grid.pixel(5, 5) == kTransparent);
    CHECK(grid.pixel(kT + 9, 5) == kTransparent);
    CHECK(grid.pixel(kT + 10, 5) == kRed);

    grid.fill({10 * kT, 0, 30, 30}, kTransparent);  // nothing there
    CHECK(grid.tile_count() == 3);
  }

  SECTION("an empty rectangle does nothing") {
    grid.fill({4, 4, 0, 10}, kRed);
    CHECK(grid.empty());
  }
}

TEST_CASE("TileGrid read matches Image::cropped", "[tiles]") {
  const Image pattern = make_test_pattern(3 * kT + 17, 2 * kT + 9);
  const Point origin{-kT - 6, -30};  // straddles the axes
  TileGrid grid;
  grid.write(pattern, origin);

  const Rect rects[] = {
      {-5, -5, 10, 10},                              // straddles four tiles
      {10, 20, 30, 40},                              // inside one tile
      {0, 0, kT, kT},                                // exactly one tile
      {origin.x - 20, origin.y - 20, 100, 100},      // hangs off the top-left
      {origin.x + 3 * kT, origin.y + 2 * kT, 90, 90},  // off the bottom-right
      {origin.x - 9, origin.y - 9, 4 * kT, 3 * kT},  // larger than the content
      {9000, 9000, 8, 8},                            // misses entirely
  };
  for (const Rect& r : rects) {
    CAPTURE(r.x, r.y, r.width, r.height);
    const Rect in_pattern{r.x - origin.x, r.y - origin.y, r.width, r.height};
    CHECK(grid.read(r) == pattern.cropped(in_pattern));
  }
}

TEST_CASE("TileGrid write matches Image::blit", "[tiles]") {
  const Image stamp = make_test_pattern(kT + 40, 90);
  const Rect window{-100, -80, 4 * kT, 3 * kT};
  const Point positions[] = {
      {-20, -45},  // straddles four tiles
      {5, 5},
      {-130, -100},  // partly outside the window
      {kT, kT},      // tile-aligned
  };
  for (const Point& p : positions) {
    CAPTURE(p.x, p.y);
    Image flat(window.width, window.height);
    TileGrid grid;
    flat.blit(stamp, {p.x - window.x, p.y - window.y});
    grid.write(stamp, p);
    CHECK(grid.read(window) == flat);
  }

  SECTION("a later write replaces what it overlaps") {
    TileGrid grid;
    grid.fill({0, 0, 2 * kT, 2 * kT}, kRed);
    grid.write(Image(10, 10, kWhite), {kT - 5, kT - 5});
    CHECK(grid.pixel(kT - 6, kT - 5) == kRed);
    CHECK(grid.pixel(kT - 5, kT - 5) == kWhite);
    CHECK(grid.pixel(kT + 4, kT + 4) == kWhite);
    CHECK(grid.pixel(kT + 5, kT + 4) == kRed);
  }
}

TEST_CASE("TileGrid copies share tiles until written", "[tiles]") {
  const Image pattern = make_test_pattern(2 * kT, 2 * kT);
  const Rect area{0, 0, 2 * kT, 2 * kT};
  TileGrid original;
  original.write(pattern, {0, 0});
  CHECK(original.tile_count() == 4);

  TileGrid copy = original;
  CHECK(copy.shared_tiles(original) == 4);

  copy.set_pixel(kT + 1, 1, kBlack);  // top-right tile
  CHECK(copy.shared_tiles(original) == 3);
  CHECK(copy.pixel(kT + 1, 1) == kBlack);
  CHECK(original.read(area) == pattern);  // the original is untouched

  // Writing to the original must not leak into the copy either.
  original.fill({0, kT, 10, 10}, kWhite);  // bottom-left tile
  CHECK(copy.shared_tiles(original) == 2);
  CHECK(copy.pixel(0, kT) == pattern.pixel(0, kT));
  CHECK(original.pixel(0, kT) == kWhite);

  // A tile this grid now owns alone is written in place, not cloned again.
  copy.set_pixel(kT + 2, 1, kBlack);
  CHECK(copy.pixel(kT + 1, 1) == kBlack);
  CHECK(copy.shared_tiles(original) == 2);

  // Tiles the copy never had are not shared.
  original.set_pixel(5 * kT, 0, kWhite);
  CHECK(copy.shared_tiles(original) == 2);
  CHECK(copy.pixel(5 * kT, 0) == kTransparent);
}

TEST_CASE("TileGrid equality compares content, not storage", "[tiles]") {
  TileGrid a, b;
  CHECK(a == b);

  a.fill({0, 0, kT, kT}, kRed);  // one shared solid block
  b.write(Image(kT, kT, kRed), {0, 0});  // separately allocated
  CHECK(a == b);
  CHECK(a.shared_tiles(b) == 0);

  b.set_pixel(kT - 1, kT - 1, kBlack);
  CHECK(a != b);
  CHECK(b != a);
  b.set_pixel(kT - 1, kT - 1, kRed);
  CHECK(a == b);

  SECTION("an allocated but transparent tile equals a missing one") {
    b.write(Image(4, 4), {3 * kT, 0});
    CHECK(b.tile_count() == 2);
    CHECK(a == b);
    CHECK(b == a);
  }

  SECTION("an extra tile with content is a difference") {
    b.set_pixel(3 * kT, 0, kWhite);
    CHECK(a != b);
    CHECK(b != a);
  }

  const TileGrid copy = a;
  CHECK(copy == a);
}
