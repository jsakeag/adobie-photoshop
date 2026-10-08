#pragma once

#include <cstddef>

#include "ps/geometry.h"
#include "ps/image.h"
#include "ps/tile_store.h"

namespace ps {

// Sparse, unbounded RGBA8 pixel storage made of 64x64 tiles.
//
// Tiles are allocated on first write; anywhere without a tile reads as
// transparent. Copies of a grid share tiles until one of them writes to a
// tile (copy-on-write), so snapshots are cheap.
class TileGrid {
 public:
  static constexpr int kTileSize = ps::kTileSize;

  bool empty() const { return store_.empty(); }
  std::size_t tile_count() const { return store_.tile_count(); }
  // Tiles whose storage is the same block in both grids.
  std::size_t shared_tiles(const TileGrid& other) const {
    return store_.shared_tiles(other.store_);
  }
  // Smallest tile-aligned rectangle covering every allocated tile.
  Rect bounds() const { return store_.bounds(); }
  // Whether any allocated tile overlaps r.
  bool any_tile_in(Rect r) const { return store_.any_tile_in(r); }

  // Any coordinate is valid, including negative ones.
  Color pixel(int x, int y) const;
  void set_pixel(int x, int y, Color c);

  // Filling with kTransparent releases the tiles it fully covers.
  void fill(Rect r, Color c);
  void clear() { store_.clear(); }

  Image read(Rect r) const;
  // Copies src with its top-left at dst. Replaces pixels, no blending.
  void write(const Image& src, Point dst);

  // Compares pixel content: an all-zero tile equals a missing one.
  bool operator==(const TileGrid& o) const { return store_ == o.store_; }
  bool operator!=(const TileGrid& o) const { return !(*this == o); }

 private:
  TileStore<Image::kChannels> store_;
};

}  // namespace ps
