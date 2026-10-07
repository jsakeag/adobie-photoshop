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

// Sparse, unbounded RGBA8 pixel storage made of 64x64 tiles.
//
// Tiles are allocated on first write; anywhere without a tile reads as
// transparent. Copies of a grid share tiles until one of them writes to a
// tile (copy-on-write), so snapshots are cheap.
class TileGrid {
 public:
  static constexpr int kTileSize = 64;

  bool empty() const { return tiles_.empty(); }
  std::size_t tile_count() const { return tiles_.size(); }
  // Tiles whose storage is the same block in both grids.
  std::size_t shared_tiles(const TileGrid& other) const;
  // Smallest tile-aligned rectangle covering every allocated tile.
  Rect bounds() const;

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
  struct Coord {
    int x = 0;
    int y = 0;
    bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
  };
  struct CoordHash {
    std::size_t operator()(const Coord& c) const {
      return std::hash<std::uint64_t>()(
          (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32) |
          static_cast<std::uint32_t>(c.y));
    }
  };
  using Tile =
      std::array<std::uint8_t, kTileSize * kTileSize * Image::kChannels>;

  const Tile* find(Coord c) const;
  // Returns a tile this grid owns alone, allocating or cloning as needed.
  Tile& writable(Coord c);

  std::unordered_map<Coord, std::shared_ptr<Tile>, CoordHash> tiles_;
};

}  // namespace ps
