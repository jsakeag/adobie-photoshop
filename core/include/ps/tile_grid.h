#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include "ps/geometry.h"
#include "ps/image.h"

namespace ps {

inline constexpr int kTileSize = 64;

// Position of a tile in tile units; tile (0, 0) covers pixels [0, 64).
struct TileCoord {
  int x = 0;
  int y = 0;
  bool operator==(const TileCoord& o) const { return x == o.x && y == o.y; }
  bool operator!=(const TileCoord& o) const { return !(*this == o); }
};

struct TileCoordHash {
  std::size_t operator()(const TileCoord& c) const {
    return std::hash<std::uint64_t>()(
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32) |
        static_cast<std::uint32_t>(c.y));
  }
};

// Tile index along one axis; rounds toward negative infinity.
inline int tile_index(int v) {
  return v >= 0 ? v / kTileSize : -((-v - 1) / kTileSize) - 1;
}

inline Rect tile_rect(TileCoord c) {
  return {c.x * kTileSize, c.y * kTileSize, kTileSize, kTileSize};
}

// Sparse, unbounded RGBA8 pixel storage made of 64x64 tiles.
//
// Tiles are allocated on first write; anywhere without a tile reads as
// transparent. Copies of a grid share tiles until one of them writes to a
// tile (copy-on-write), so snapshots are cheap.
class TileGrid {
 public:
  static constexpr int kTileSize = ps::kTileSize;

  bool empty() const { return tiles_.empty(); }
  std::size_t tile_count() const { return tiles_.size(); }
  // Tiles whose storage is the same block in both grids.
  std::size_t shared_tiles(const TileGrid& other) const;
  // Smallest tile-aligned rectangle covering every allocated tile.
  Rect bounds() const;
  // Whether any allocated tile overlaps r.
  bool any_tile_in(Rect r) const;

  // Any coordinate is valid, including negative ones.
  Color pixel(int x, int y) const;
  void set_pixel(int x, int y, Color c);

  // Filling with kTransparent releases the tiles it fully covers.
  void fill(Rect r, Color c);
  void clear() { tiles_.clear(); }

  Image read(Rect r) const;
  // Copies src with its top-left at dst. Replaces pixels, no blending.
  void write(const Image& src, Point dst);

  // Compares pixel content: an all-zero tile equals a missing one.
  bool operator==(const TileGrid& o) const;
  bool operator!=(const TileGrid& o) const { return !(*this == o); }

 private:
  using Tile =
      std::array<std::uint8_t, kTileSize * kTileSize * Image::kChannels>;

  const Tile* find(TileCoord c) const;
  // Returns a tile this grid owns alone, allocating or cloning as needed.
  Tile& writable(TileCoord c);

  std::unordered_map<TileCoord, std::shared_ptr<Tile>, TileCoordHash>
      tiles_;
};

}  // namespace ps
