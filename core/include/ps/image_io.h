#pragma once

#include <string>

#include "ps/image.h"

namespace ps {

// Loads PNG, JPEG, BMP, TGA or GIF as RGBA8. Throws std::runtime_error.
Image load_image(const std::string& path);

// Throws std::runtime_error if the file can't be written.
void save_png(const Image& image, const std::string& path);

}  // namespace ps
