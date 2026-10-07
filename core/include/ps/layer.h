#pragma once

#include <string>

#include "ps/blend.h"
#include "ps/geometry.h"
#include "ps/tile_grid.h"

namespace ps {

struct Layer {
  std::string name;
  TileGrid pixels;
  Point offset;  // where the layer's pixel (0, 0) sits in the document
  float opacity = 1.0f;  // 0..1
  bool visible = true;
  BlendMode blend = BlendMode::Normal;

  // Tile-aligned extent of the layer's pixels, in document coordinates.
  Rect bounds() const {
    const Rect b = pixels.bounds();
    return {b.x + offset.x, b.y + offset.y, b.width, b.height};
  }
};

}  // namespace ps
