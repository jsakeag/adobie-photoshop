#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ps/geometry.h"

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

// Sparse, unbounded storage of 8-bit pixels with N channels, in 64x64 tiles.
// The shared machinery behind TileGrid (RGBA) and MaskGrid (one channel).
//
// Tiles are allocated on first write; anywhere without a tile reads as the
// background pixel. Copies share tiles until one of them writes to a tile
// (copy-on-write), so snapshots are cheap.
template <int N>
class TileStore {
 public:
  using Pixel = std::array<std::uint8_t, N>;

  explicit TileStore(Pixel background = {}) : background_(background) {}

  const Pixel& background() const { return background_; }
  bool empty() const { return tiles_.empty(); }
  std::size_t tile_count() const { return tiles_.size(); }
  void clear() { tiles_.clear(); }

  // Tiles whose storage is the same block in both stores.
  std::size_t shared_tiles(const TileStore& other) const {
    std::size_t count = 0;
    for (const auto& entry : tiles_) {
      count += other.find(entry.first) == entry.second.get();
    }
    return count;
  }

  // Tiles that are not the same block in both stores, including tiles only
  // one of them has. Tiles left out are guaranteed to hold equal pixels.
  std::vector<TileCoord> changed_tiles(const TileStore& other) const {
    std::vector<TileCoord> changed;
    for (const auto& entry : tiles_) {
      if (other.find(entry.first) != entry.second.get()) {
        changed.push_back(entry.first);
      }
    }
    for (const auto& entry : other.tiles_) {
      if (!find(entry.first)) changed.push_back(entry.first);
    }
    return changed;
  }

  // Smallest tile-aligned rectangle covering every allocated tile.
  Rect bounds() const {
    if (tiles_.empty()) return {};
    int x0 = tiles_.begin()->first.x, x1 = x0;
    int y0 = tiles_.begin()->first.y, y1 = y0;
    for (const auto& entry : tiles_) {
      x0 = std::min(x0, entry.first.x);
      x1 = std::max(x1, entry.first.x);
      y0 = std::min(y0, entry.first.y);
      y1 = std::max(y1, entry.first.y);
    }
    return {x0 * kTileSize, y0 * kTileSize, (x1 - x0 + 1) * kTileSize,
            (y1 - y0 + 1) * kTileSize};
  }

  // Whether any allocated tile overlaps r.
  bool any_tile_in(Rect r) const {
    if (r.empty() || tiles_.empty()) return false;
    const int tx0 = tile_index(r.x), tx1 = tile_index(r.right() - 1);
    const int ty0 = tile_index(r.y), ty1 = tile_index(r.bottom() - 1);
    // Walk whichever is smaller: the tiles under r or the allocated tiles.
    const long long covered =
        static_cast<long long>(tx1 - tx0 + 1) * (ty1 - ty0 + 1);
    if (covered <= static_cast<long long>(tiles_.size())) {
      for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
          if (find({tx, ty})) return true;
        }
      }
      return false;
    }
    for (const auto& entry : tiles_) {
      const TileCoord c = entry.first;
      if (c.x >= tx0 && c.x <= tx1 && c.y >= ty0 && c.y <= ty1) return true;
    }
    return false;
  }

  Pixel pixel(int x, int y) const {
    const TileCoord c{tile_index(x), tile_index(y)};
    const Tile* tile = find(c);
    if (!tile) return background_;
    Pixel p;
    std::memcpy(p.data(), tile->data() + offset(x, y, c), N);
    return p;
  }

  void set_pixel(int x, int y, Pixel p) {
    const TileCoord c{tile_index(x), tile_index(y)};
    std::memcpy(writable(c).data() + offset(x, y, c), p.data(), N);
  }

  // Filling with the background pixel releases the tiles it fully covers.
  void fill(Rect r, Pixel p) {
    if (r.empty()) return;
    const bool erase = p == background_;
    // Every fully covered tile points at this one block until it is written.
    std::shared_ptr<Tile> solid;
    for (int ty = tile_index(r.y); ty <= tile_index(r.bottom() - 1); ++ty) {
      for (int tx = tile_index(r.x); tx <= tile_index(r.right() - 1); ++tx) {
        const TileCoord c{tx, ty};
        const Rect part = tile_rect(c).intersect(r);
        if (part == tile_rect(c)) {
          if (erase) {
            tiles_.erase(c);
            continue;
          }
          if (!solid) solid = new_tile(p);
          tiles_[c] = solid;
          continue;
        }
        if (erase && !find(c)) continue;
        Tile& tile = writable(c);
        for (int y = part.y; y < part.bottom(); ++y) {
          fill_run(tile.data() + offset(part.x, y, c), part.width, p);
        }
      }
    }
  }

  // Copies the pixels under r into `out`: r.width * r.height pixels, row by
  // row, N bytes each.
  void read(Rect r, std::uint8_t* out) const {
    if (r.empty()) return;
    fill_run(out, static_cast<std::size_t>(r.width) * r.height, background_);
    for (int ty = tile_index(r.y); ty <= tile_index(r.bottom() - 1); ++ty) {
      for (int tx = tile_index(r.x); tx <= tile_index(r.right() - 1); ++tx) {
        const TileCoord c{tx, ty};
        const Tile* tile = find(c);
        if (!tile) continue;
        const Rect part = tile_rect(c).intersect(r);
        for (int y = part.y; y < part.bottom(); ++y) {
          std::memcpy(out + flat_offset(part.x, y, r),
                      tile->data() + offset(part.x, y, c),
                      static_cast<std::size_t>(part.width) * N);
        }
      }
    }
  }

  // Replaces the pixels under r with `src`, laid out as read() produces.
  void write(const std::uint8_t* src, Rect r) {
    if (r.empty()) return;
    for (int ty = tile_index(r.y); ty <= tile_index(r.bottom() - 1); ++ty) {
      for (int tx = tile_index(r.x); tx <= tile_index(r.right() - 1); ++tx) {
        const TileCoord c{tx, ty};
        const Rect part = tile_rect(c).intersect(r);
        Tile& tile = writable(c);
        for (int y = part.y; y < part.bottom(); ++y) {
          std::memcpy(tile.data() + offset(part.x, y, c),
                      src + flat_offset(part.x, y, r),
                      static_cast<std::size_t>(part.width) * N);
        }
      }
    }
  }

  // Compares pixel content: a tile of background pixels equals a missing one.
  bool operator==(const TileStore& o) const {
    if (background_ != o.background_) return false;
    for (const auto& entry : tiles_) {
      const Tile* theirs = o.find(entry.first);
      if (theirs == entry.second.get()) continue;
      if (theirs ? *theirs != *entry.second : !is_background(*entry.second)) {
        return false;
      }
    }
    for (const auto& entry : o.tiles_) {
      if (!find(entry.first) && !is_background(*entry.second)) return false;
    }
    return true;
  }
  bool operator!=(const TileStore& o) const { return !(*this == o); }

 private:
  static constexpr std::size_t kPixels =
      static_cast<std::size_t>(kTileSize) * kTileSize;
  using Tile = std::array<std::uint8_t, kTileSize * kTileSize * N>;

  // Byte offset of pixel (x, y) inside tile c.
  static std::size_t offset(int x, int y, TileCoord c) {
    return (static_cast<std::size_t>(y - c.y * kTileSize) * kTileSize +
            (x - c.x * kTileSize)) *
           N;
  }

  // Byte offset of pixel (x, y) inside a flat buffer covering r.
  static std::size_t flat_offset(int x, int y, Rect r) {
    return (static_cast<std::size_t>(y - r.y) * r.width + (x - r.x)) * N;
  }

  static void fill_run(std::uint8_t* p, std::size_t count, const Pixel& px) {
    for (std::size_t i = 0; i < count; ++i) {
      std::memcpy(p + i * N, px.data(), N);
    }
  }

  static std::shared_ptr<Tile> new_tile(const Pixel& px) {
    auto tile = std::make_shared<Tile>();
    fill_run(tile->data(), kPixels, px);
    return tile;
  }

  bool is_background(const Tile& tile) const {
    for (std::size_t i = 0; i < kPixels; ++i) {
      if (std::memcmp(tile.data() + i * N, background_.data(), N) != 0) {
        return false;
      }
    }
    return true;
  }

  const Tile* find(TileCoord c) const {
    const auto it = tiles_.find(c);
    return it == tiles_.end() ? nullptr : it->second.get();
  }

  // Returns a tile this store owns alone, allocating or cloning as needed.
  Tile& writable(TileCoord c) {
    std::shared_ptr<Tile>& tile = tiles_[c];
    if (!tile) {
      tile = new_tile(background_);
    } else if (tile.use_count() > 1) {
      tile = std::make_shared<Tile>(*tile);
    }
    return *tile;
  }

  Pixel background_;
  std::unordered_map<TileCoord, std::shared_ptr<Tile>, TileCoordHash> tiles_;
};

}  // namespace ps
