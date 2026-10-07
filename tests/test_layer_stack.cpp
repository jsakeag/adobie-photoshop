#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ps/layer_stack.h"

using namespace ps;

namespace {

Layer named(const std::string& name) {
  Layer layer;
  layer.name = name;
  return layer;
}

std::string order(const LayerStack& stack) {
  std::string names;
  for (const Layer& layer : stack) names += layer.name;
  return names;
}

LayerStack abcd() {
  LayerStack stack;
  for (const char* name : {"a", "b", "c", "d"}) {
    stack.insert(stack.size(), named(name));
  }
  return stack;
}

}  // namespace

TEST_CASE("Layer defaults", "[layers]") {
  const Layer layer;
  CHECK(layer.opacity == 1.0f);
  CHECK(layer.visible);
  CHECK(layer.blend == BlendMode::Normal);
  CHECK(layer.bounds().empty());
}

TEST_CASE("Layer bounds follow its offset", "[layers]") {
  Layer layer;
  layer.pixels.set_pixel(70, 5, kWhite);  // tile (1, 0)
  layer.offset = {-10, 30};
  CHECK(layer.bounds() == Rect{kTileSize - 10, 30, kTileSize, kTileSize});
}

TEST_CASE("LayerStack insert", "[layers]") {
  LayerStack stack;
  CHECK(stack.empty());

  stack.insert(0, named("b"));
  stack.insert(0, named("a"));  // below b
  stack.insert(2, named("d"));  // on top
  stack.insert(2, named("c"));  // below d
  CHECK(stack.size() == 4);
  CHECK(order(stack) == "abcd");
  CHECK(stack.at(0).name == "a");
  CHECK(stack.at(3).name == "d");

  CHECK_THROWS_AS(stack.insert(6, named("x")), std::out_of_range);
  CHECK_THROWS_AS(stack.at(4), std::out_of_range);
  CHECK(order(stack) == "abcd");
}

TEST_CASE("LayerStack remove", "[layers]") {
  LayerStack stack = abcd();
  CHECK(stack.remove(1).name == "b");
  CHECK(order(stack) == "acd");
  CHECK(stack.remove(2).name == "d");
  CHECK(order(stack) == "ac");
  CHECK_THROWS_AS(stack.remove(2), std::out_of_range);
  CHECK(order(stack) == "ac");
}

TEST_CASE("LayerStack move", "[layers]") {
  LayerStack stack = abcd();

  SECTION("upward") {
    stack.move(0, 2);
    CHECK(order(stack) == "bcad");
  }
  SECTION("to the top") {
    stack.move(1, 3);
    CHECK(order(stack) == "acdb");
  }
  SECTION("downward") {
    stack.move(3, 1);
    CHECK(order(stack) == "adbc");
  }
  SECTION("to the bottom") {
    stack.move(2, 0);
    CHECK(order(stack) == "cabd");
  }
  SECTION("onto itself") {
    stack.move(2, 2);
    CHECK(order(stack) == "abcd");
  }
  SECTION("out of range") {
    CHECK_THROWS_AS(stack.move(4, 0), std::out_of_range);
    CHECK_THROWS_AS(stack.move(0, 4), std::out_of_range);
    CHECK(order(stack) == "abcd");
  }
}
