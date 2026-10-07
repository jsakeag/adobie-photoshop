#include "golden.h"

#include <cstdint>
#include <cstdlib>
#include <exception>

#include "ps/image_io.h"

std::string golden_mismatch(const ps::Image& actual, const std::string& name) {
  const std::string golden_path = std::string(PS_GOLDEN_DIR) + "/" + name + ".png";
  if (std::getenv("PS_UPDATE_GOLDEN")) {
    ps::save_png(actual, golden_path);
    return {};
  }

  std::string problem;
  try {
    const ps::Image expected = ps::load_image(golden_path);
    if (expected == actual) return {};
    if (expected.width() != actual.width() ||
        expected.height() != actual.height()) {
      problem = "size differs from golden";
    } else {
      problem = "pixels differ from golden";
    }
  } catch (const std::exception& e) {
    problem = e.what();
  }

  const std::string actual_path =
      std::string(PS_TEST_OUTPUT_DIR) + "/" + name + ".actual.png";
  ps::save_png(actual, actual_path);
  return name + ": " + problem + " (actual written to " + actual_path + ")";
}

ps::Image make_test_pattern(int width, int height) {
  ps::Image image(width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto r = static_cast<std::uint8_t>(x * 255 / (width - 1));
      const auto g = static_cast<std::uint8_t>(y * 255 / (height - 1));
      const auto a = static_cast<std::uint8_t>(64 + x * 191 / (width - 1));
      image.set_pixel(x, y, {r, g, 128, a});
    }
  }
  image.fill({0, 0, 4, 4}, ps::kBlack);
  image.fill({width - 4, 0, 4, 4}, ps::kWhite);
  image.fill({0, height - 4, 4, 4}, {255, 0, 255, 255});
  return image;
}
