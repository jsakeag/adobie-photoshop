#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/compositor.h"

using namespace ps;

namespace {

constexpr int kT = kTileSize;
const Color kRed{255, 0, 0, 255};
const Color kBlue{0, 0, 255, 255};

Layer solid(Rect area, Color c) {
  Layer layer;
  layer.pixels.fill(area, c);
  return layer;
}

}  // namespace

TEST_CASE("Compositing an empty stack gives transparency", "[compositor]") {
  Compositor compositor;
  const LayerStack stack;
  CHECK(compositor.render(stack, {-5, -5, 20, 10}) == Image(20, 10));
  CHECK(compositor.render(stack, {0, 0, 0, 0}).empty());
  CHECK_THROWS_AS(compositor.render(stack, {0, 0, -1, 4}),
                  std::invalid_argument);
}

TEST_CASE("A single layer composites to its own pixels", "[compositor]") {
  const Image pattern = make_test_pattern(2 * kT + 30, kT + 11);
  Layer layer;
  layer.pixels.write(pattern, {0, 0});
  LayerStack stack;
  stack.insert(0, layer);

  Compositor compositor;
  CHECK(compositor.render(stack, {0, 0, pattern.width(), pattern.height()}) ==
        pattern);
  // A window that straddles tiles and runs off the layer.
  const Rect window{kT - 7, kT - 9, 90, 40};
  CHECK(compositor.render(stack, window) == pattern.cropped(window));
}

TEST_CASE("Layer offset places the layer in the document", "[compositor]") {
  const Image pattern = make_test_pattern(40, 30);
  Layer layer;
  layer.pixels.write(pattern, {0, 0});
  layer.offset = {-15, kT + 3};
  LayerStack stack;
  stack.insert(0, layer);

  Compositor compositor;
  const Rect window{-30, kT - 10, 80, 60};
  Image expected(window.width, window.height);
  expected.blit(pattern, {layer.offset.x - window.x, layer.offset.y - window.y});
  CHECK(compositor.render(stack, window) == expected);
}

TEST_CASE("Layers composite bottom to top", "[compositor]") {
  LayerStack stack;
  stack.insert(0, solid({0, 0, 20, 20}, kRed));
  stack.insert(1, solid({10, 0, 20, 20}, kBlue));

  Compositor compositor;
  const Image result = compositor.render(stack, {0, 0, 40, 20});
  CHECK(result.pixel(5, 5) == kRed);
  CHECK(result.pixel(15, 5) == kBlue);  // the overlap shows the top layer
  CHECK(result.pixel(25, 5) == kBlue);
  CHECK(result.pixel(35, 5) == kTransparent);

  SECTION("reordering swaps who wins the overlap") {
    stack.move(1, 0);
    compositor.invalidate_all();
    CHECK(compositor.render(stack, {0, 0, 40, 20}).pixel(15, 5) == kRed);
  }
}

TEST_CASE("Hidden and fully transparent layers are skipped", "[compositor]") {
  LayerStack stack;
  stack.insert(0, solid({0, 0, 20, 20}, kRed));
  stack.insert(1, solid({0, 0, 20, 20}, kBlue));
  Compositor compositor;

  SECTION("hidden") {
    stack.at(1).visible = false;
    CHECK(compositor.render(stack, {0, 0, 20, 20}) == Image(20, 20, kRed));
  }
  SECTION("zero opacity") {
    stack.at(1).opacity = 0.0f;
    CHECK(compositor.render(stack, {0, 0, 20, 20}) == Image(20, 20, kRed));
  }
  SECTION("everything hidden") {
    stack.at(0).visible = false;
    stack.at(1).visible = false;
    CHECK(compositor.render(stack, {0, 0, 20, 20}) == Image(20, 20));
  }
}

TEST_CASE("Layer opacity scales its contribution", "[compositor]") {
  LayerStack stack;
  stack.insert(0, solid({0, 0, 10, 10}, kWhite));
  stack.insert(1, solid({0, 0, 10, 10}, kBlack));
  Compositor compositor;

  stack.at(1).opacity = 0.5f;
  CHECK(compositor.render(stack, {0, 0, 10, 10}) ==
        Image(10, 10, {128, 128, 128, 255}));

  SECTION("alone, a translucent layer leaves translucent pixels") {
    stack.remove(0);
    compositor.invalidate_all();
    CHECK(compositor.render(stack, {0, 0, 10, 10}) ==
          Image(10, 10, {0, 0, 0, 128}));
  }
}

TEST_CASE("Compositor caches tiles until invalidated", "[compositor]") {
  LayerStack stack;
  stack.insert(0, solid({0, 0, 4 * kT, 2 * kT}, kRed));
  Compositor compositor;
  const Rect all{0, 0, 4 * kT, 2 * kT};

  compositor.render(stack, all);
  CHECK(compositor.tiles_composited() == 8);

  compositor.render(stack, all);
  compositor.render(stack, {10, 10, kT, kT});
  CHECK(compositor.tiles_composited() == 8);  // all served from the cache

  SECTION("only requested tiles are composited") {
    Compositor fresh;
    fresh.render(stack, {kT + 5, 5, 10, 10});
    CHECK(fresh.tiles_composited() == 1);
    fresh.render(stack, {kT - 1, kT - 1, 2, 2});  // touches four, one cached
    CHECK(fresh.tiles_composited() == 4);
  }

  SECTION("without invalidation a change is not seen") {
    stack.at(0).pixels.set_pixel(5, 5, kBlue);
    CHECK(compositor.render(stack, all).pixel(5, 5) == kRed);
  }

  SECTION("invalidating a small area recomposites only its tiles") {
    stack.at(0).pixels.fill({kT - 2, 5, 4, 4}, kBlue);  // spans two tiles
    compositor.invalidate({kT - 2, 5, 4, 4});
    const Image result = compositor.render(stack, all);
    CHECK(compositor.tiles_composited() == 10);
    CHECK(result.pixel(kT - 2, 5) == kBlue);
    CHECK(result.pixel(kT + 1, 8) == kBlue);
    CHECK(result.pixel(kT + 2, 8) == kRed);
  }

  SECTION("an empty rectangle invalidates nothing") {
    compositor.invalidate({5, 5, 0, 0});
    compositor.render(stack, all);
    CHECK(compositor.tiles_composited() == 8);
  }

  SECTION("a huge rectangle invalidates every cached tile") {
    compositor.invalidate({-1000000, -1000000, 2000000, 2000000});
    compositor.render(stack, all);
    CHECK(compositor.tiles_composited() == 16);
  }

  SECTION("a tile that becomes empty is cleared") {
    stack.at(0).pixels.fill({0, 0, kT, kT}, kTransparent);
    compositor.invalidate({0, 0, kT, kT});
    const Image result = compositor.render(stack, all);
    CHECK(result.pixel(5, 5) == kTransparent);
    CHECK(result.pixel(kT, 5) == kRed);
  }

  SECTION("invalidate_all drops everything") {
    compositor.invalidate_all();
    compositor.render(stack, all);
    CHECK(compositor.tiles_composited() == 16);
  }
}
