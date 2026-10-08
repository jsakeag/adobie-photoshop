#include <cstdint>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ps/mask_grid.h"

using namespace ps;

namespace {

constexpr int kT = kTileSize;

// Distinct value per position, so misplaced bytes show up.
std::vector<std::uint8_t> ramp(Rect r) {
  std::vector<std::uint8_t> values;
  for (int y = 0; y < r.height; ++y) {
    for (int x = 0; x < r.width; ++x) {
      values.push_back(static_cast<std::uint8_t>(1 + (x * 7 + y * 13) % 250));
    }
  }
  return values;
}

}  // namespace

TEST_CASE("A new MaskGrid reads as its background everywhere", "[mask]") {
  const MaskGrid hide_all;
  CHECK(hide_all.background() == 0);
  CHECK(hide_all.empty());
  CHECK(hide_all.value(0, 0) == 0);
  CHECK(hide_all.value(-9000, 4321) == 0);

  const MaskGrid reveal_all(255);
  CHECK(reveal_all.background() == 255);
  CHECK(reveal_all.tile_count() == 0);
  CHECK(reveal_all.value(-9000, 4321) == 255);
  CHECK(reveal_all.read({-3, -3, 4, 2}) ==
        std::vector<std::uint8_t>(8, 255));
  CHECK(reveal_all.read({0, 0, 0, 0}).empty());
  CHECK_THROWS_AS(reveal_all.read({0, 0, -1, 2}), std::invalid_argument);
}

TEST_CASE("MaskGrid allocates tiles on write", "[mask]") {
  MaskGrid mask(255);
  mask.set_value(-1, -1, 10);
  CHECK(mask.tile_count() == 1);
  CHECK(mask.value(-1, -1) == 10);
  CHECK(mask.value(-2, -1) == 255);  // the rest of the tile is background
  CHECK(mask.bounds() == Rect{-kT, -kT, kT, kT});
  CHECK(mask.any_tile_in({-5, -5, 3, 3}));
  CHECK_FALSE(mask.any_tile_in({0, 0, 50, 50}));

  mask.clear();
  CHECK(mask.empty());
  CHECK(mask.value(-1, -1) == 255);
}

TEST_CASE("MaskGrid fill", "[mask]") {
  MaskGrid mask(255);
  mask.fill({kT - 4, 0, 8, 8}, 0);  // straddles two tiles
  CHECK(mask.tile_count() == 2);
  CHECK(mask.value(kT - 5, 0) == 255);
  CHECK(mask.value(kT - 4, 0) == 0);
  CHECK(mask.value(kT + 3, 7) == 0);
  CHECK(mask.value(kT + 4, 7) == 255);

  SECTION("filling whole tiles with the background releases them") {
    mask.fill({0, 0, 2 * kT, kT}, 255);
    CHECK(mask.empty());
  }

  SECTION("filling with the background allocates nothing") {
    mask.fill({10 * kT, 0, 20, 20}, 255);
    CHECK(mask.tile_count() == 2);
  }
}

TEST_CASE("MaskGrid read and write round-trip across tiles", "[mask]") {
  const Rect area{-20, kT - 10, kT + 50, 40};
  const std::vector<std::uint8_t> values = ramp(area);
  MaskGrid mask(255);
  mask.write(values, area);
  CHECK(mask.read(area) == values);
  CHECK(mask.value(area.x, area.y) == values.front());
  CHECK(mask.value(area.right() - 1, area.bottom() - 1) == values.back());
  CHECK(mask.value(area.x - 1, area.y) == 255);

  // A wider read sees the background around the written block.
  const std::vector<std::uint8_t> wider =
      mask.read({area.x - 1, area.y, area.width + 2, 1});
  CHECK(wider.front() == 255);
  CHECK(wider[1] == values[0]);
  CHECK(wider.back() == 255);

  CHECK_THROWS_AS(mask.write(values, {0, 0, 3, 3}), std::invalid_argument);
}

TEST_CASE("MaskGrid copies share tiles until written", "[mask]") {
  MaskGrid original;
  original.fill({0, 0, 10, 10}, 200);
  original.fill({kT, 0, 10, 10}, 200);

  MaskGrid copy = original;
  CHECK(copy.shared_tiles(original) == 2);
  copy.set_value(0, 0, 5);
  CHECK(copy.shared_tiles(original) == 1);
  CHECK(original.value(0, 0) == 200);
  CHECK(copy.value(0, 0) == 5);
}

TEST_CASE("MaskGrid equality compares content", "[mask]") {
  MaskGrid a(255), b(255);
  CHECK(a == b);
  a.fill({0, 0, 5, 5}, 9);
  CHECK(a != b);
  b.fill({0, 0, 5, 5}, 9);
  CHECK(a == b);

  // A tile holding only background values equals a missing one.
  a.set_value(3 * kT, 0, 1);
  a.set_value(3 * kT, 0, 255);
  CHECK(a.tile_count() == 2);
  CHECK(a == b);
  CHECK(b == a);

  // Same tiles, different background: different masks.
  CHECK(MaskGrid(0) != MaskGrid(255));
}
