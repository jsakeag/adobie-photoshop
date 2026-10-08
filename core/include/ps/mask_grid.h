#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ps/geometry.h"
#include "ps/tile_store.h"

namespace ps {

// Sparse, unbounded 8-bit single-channel storage made of 64x64 tiles: layer
// masks now, selections later. 0 hides, 255 shows.
//
// Anywhere without a tile reads as the background value, so a new mask
// either reveals everything (255) or hides everything (0) at no cost.
// Copy-on-write like TileGrid.
class MaskGrid {
 public:
  explicit MaskGrid(std::uint8_t background = 0) : store_({background}) {}

  std::uint8_t background() const { return store_.background()[0]; }
  bool empty() const { return store_.empty(); }
  std::size_t tile_count() const { return store_.tile_count(); }
  std::size_t shared_tiles(const MaskGrid& other) const {
    return store_.shared_tiles(other.store_);
  }
  Rect bounds() const { return store_.bounds(); }
  bool any_tile_in(Rect r) const { return store_.any_tile_in(r); }

  // Any coordinate is valid, including negative ones.
  std::uint8_t value(int x, int y) const { return store_.pixel(x, y)[0]; }
  void set_value(int x, int y, std::uint8_t v) { store_.set_pixel(x, y, {v}); }

  // Filling with the background value releases the tiles it fully covers.
  void fill(Rect r, std::uint8_t v) { store_.fill(r, {v}); }
  void clear() { store_.clear(); }

  // The values under r, row by row: r.width * r.height bytes.
  std::vector<std::uint8_t> read(Rect r) const;
  // Replaces the values under r. Throws std::invalid_argument unless
  // `values` holds exactly r.width * r.height bytes.
  void write(const std::vector<std::uint8_t>& values, Rect r);

  // Compares content: a tile of background values equals a missing one.
  bool operator==(const MaskGrid& o) const { return store_ == o.store_; }
  bool operator!=(const MaskGrid& o) const { return !(*this == o); }

 private:
  TileStore<1> store_;
};

}  // namespace ps
