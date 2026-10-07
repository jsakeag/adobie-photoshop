#include "ps/image_io.h"

#include <cstring>
#include <stdexcept>

// Static so these don't clash with the stb copy bundled inside NanoGUI.
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "stb_image.h"
#include "stb_image_write.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace ps {

Image load_image(const std::string& path) {
  int width = 0, height = 0, channels = 0;
  stbi_uc* pixels =
      stbi_load(path.c_str(), &width, &height, &channels, Image::kChannels);
  if (!pixels) {
    throw std::runtime_error("Could not load '" + path +
                             "': " + stbi_failure_reason());
  }
  Image image(width, height);
  std::memcpy(image.data(), pixels, image.size_bytes());
  stbi_image_free(pixels);
  return image;
}

void save_png(const Image& image, const std::string& path) {
  if (image.empty()) throw std::runtime_error("Cannot save an empty image");
  const int ok =
      stbi_write_png(path.c_str(), image.width(), image.height(),
                     Image::kChannels, image.data(),
                     image.width() * Image::kChannels);
  if (!ok) throw std::runtime_error("Could not write '" + path + "'");
}

}  // namespace ps
