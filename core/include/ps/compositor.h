#pragma once

#include <cstddef>
#include <unordered_set>

#include "ps/geometry.h"
#include "ps/image.h"
#include "ps/layer_stack.h"
#include "ps/tile_grid.h"

namespace ps {

// Flattens a LayerStack, bottom to top, onto transparent.
//
// Results are cached per 64x64 document tile. The cache does not watch the
// layers: whoever changes them must invalidate() the affected area, and only
// those tiles are composited again.
class Compositor {
 public:
  // The composite under r, in document coordinates.
  Image render(const LayerStack& stack, Rect r);

  // Marks cached tiles that overlap r as stale.
  void invalidate(Rect r);
  void invalidate_all();

  // Number of tiles composited so far; lets tests observe the cache.
  std::size_t tiles_composited() const { return tiles_composited_; }

 private:
  void composite_tile(const LayerStack& stack, TileCoord coord);

  TileGrid cache_;
  std::unordered_set<TileCoord, TileCoordHash> valid_;
  std::size_t tiles_composited_ = 0;
};

}  // namespace ps
