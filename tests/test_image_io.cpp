#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/image_io.h"

using namespace ps;

TEST_CASE("PNG round trip preserves every channel", "[io]") {
  const Image original = make_test_pattern(33, 17);
  const std::string path = std::string(PS_TEST_OUTPUT_DIR) + "/round_trip.png";

  save_png(original, path);
  CHECK(load_image(path) == original);
}

TEST_CASE("Image I/O reports failures", "[io]") {
  CHECK_THROWS_AS(load_image("/nonexistent/missing.png"), std::runtime_error);
  CHECK_THROWS_AS(save_png(Image(2, 2), "/nonexistent/dir/out.png"),
                  std::runtime_error);
  CHECK_THROWS_AS(save_png(Image(), std::string(PS_TEST_OUTPUT_DIR) + "/e.png"),
                  std::runtime_error);
}
