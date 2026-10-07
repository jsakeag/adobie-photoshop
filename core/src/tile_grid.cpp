#include "ps/tile_grid.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ps {

namespace {

constexpr int kTile = TileGrid::kTileSize;
constexpr int kChannels = Image::kChannels;

// Byte offset of grid pixel (x, y) inside tile (tx, ty).
std::size_t tile_offset(int x, int y, int tx, int ty) {
  return (static_cast<std::size_t>(y - ty * kTile) * kTile + (x - tx * kTile)) *
         kChannels;
}

Rect tile_rect(int tx, int ty) { return tile_rect(TileCoord{tx, ty}); }

void fill_pixels(std::uint8_t* p, std::size_t count, Color c) {
  const std::uint8_t px[kChannels] = {c.r, c.g, c.b, c.a};
  for (std::size_t i = 0; i < count; ++i) {
    std::memcpy(p + i * kChannels, px, kChannels);
  }
}

template <typename TileT>
bool all_zero(const TileT& tile) {
  return std::all_of(tile.begin(), tile.end(),
                     [](std::uint8_t v) { return v == 0; });
}

}  // namespace

const TileGrid::Tile* TileGrid::find(TileCoord c) const {
  const auto it = tiles_.find(c);
  return it == tiles_.end() ? nullptr : it->second.get();
}

TileGrid::Tile& TileGrid::writable(TileCoord c) {
  std::shared_ptr<Tile>& tile = tiles_[c];
  if (!tile) {
    tile = std::make_shared<Tile>();  // value-initialised: transparent
  } else if (tile.use_count() > 1) {
    tile = std::make_shared<Tile>(*tile);
  }
  return *tile;
}

std::size_t TileGrid::shared_tiles(const TileGrid& other) const {
  std::size_t count = 0;
  for (const auto& entry : tiles_) {
    count += other.find(entry.first) == entry.second.get();
  }
  return count;
}

Rect TileGrid::bounds() const {
  if (tiles_.empty()) return {};
  int x0 = tiles_.begin()->first.x, x1 = x0;
  int y0 = tiles_.begin()->first.y, y1 = y0;
  for (const auto& entry : tiles_) {
    x0 = std::min(x0, entry.first.x);
    x1 = std::max(x1, entry.first.x);
    y0 = std::min(y0, entry.first.y);
    y1 = std::max(y1, entry.first.y);
  }
  return {x0 * kTile, y0 * kTile, (x1 - x0 + 1) * kTile, (y1 - y0 + 1) * kTile};
}

bool TileGrid::any_tile_in(Rect r) const {
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

Color TileGrid::pixel(int x, int y) const {
  const int tx = tile_index(x), ty = tile_index(y);
  const Tile* tile = find({tx, ty});
  if (!tile) return kTransparent;
  const std::uint8_t* p = tile->data() + tile_offset(x, y, tx, ty);
  return {p[0], p[1], p[2], p[3]};
}

void TileGrid::set_pixel(int x, int y, Color c) {
  const int tx = tile_index(x), ty = tile_index(y);
  fill_pixels(writable({tx, ty}).data() + tile_offset(x, y, tx, ty), 1, c);
}

void TileGrid::fill(Rect r, Color c) {
  if (r.empty()) return;
  const bool erase = c == kTransparent;
  // Every fully covered tile points at this one block until it is written.
  std::shared_ptr<Tile> solid;
  for (int ty = tile_index(r.y); ty <= tile_index(r.bottom() - 1); ++ty) {
    for (int tx = tile_index(r.x); tx <= tile_index(r.right() - 1); ++tx) {
      const Rect part = tile_rect(tx, ty).intersect(r);
      if (part == tile_rect(tx, ty)) {
        if (erase) {
          tiles_.erase({tx, ty});
          continue;
        }
        if (!solid) {
          solid = std::make_shared<Tile>();
          fill_pixels(solid->data(), static_cast<std::size_t>(kTile) * kTile, c);
        }
        tiles_[{tx, ty}] = solid;
        continue;
      }
      if (erase && !find({tx, ty})) continue;
      Tile& tile = writable({tx, ty});
      for (int y = part.y; y < part.bottom(); ++y) {
        fill_pixels(tile.data() + tile_offset(part.x, y, tx, ty), part.width, c);
      }
    }
  }
}

Image TileGrid::read(Rect r) const {
  if (r.width < 0 || r.height < 0) {
    throw std::invalid_argument("TileGrid::read needs a non-negative size");
  }
  Image out(r.width, r.height);
  if (r.empty()) return out;
  for (int ty = tile_index(r.y); ty <= tile_index(r.bottom() - 1); ++ty) {
    for (int tx = tile_index(r.x); tx <= tile_index(r.right() - 1); ++tx) {
      const Tile* tile = find({tx, ty});
      if (!tile) continue;
      const Rect part = tile_rect(tx, ty).intersect(r);
      for (int y = part.y; y < part.bottom(); ++y) {
        const std::size_t to =
            (static_cast<std::size_t>(y - r.y) * r.width + (part.x - r.x)) *
            kChannels;
        std::memcpy(out.data() + to,
                    tile->data() + tile_offset(part.x, y, tx, ty),
                    static_cast<std::size_t>(part.width) * kChannels);
      }
    }
  }
  return out;
}

void TileGrid::write(const Image& src, Point dst) {
  const Rect r{dst.x, dst.y, src.width(), src.height()};
  if (r.empty()) return;
  for (int ty = tile_index(r.y); ty <= tile_index(r.bottom() - 1); ++ty) {
    for (int tx = tile_index(r.x); tx <= tile_index(r.right() - 1); ++tx) {
      const Rect part = tile_rect(tx, ty).intersect(r);
      Tile& tile = writable({tx, ty});
      for (int y = part.y; y < part.bottom(); ++y) {
        const std::size_t from =
            (static_cast<std::size_t>(y - r.y) * r.width + (part.x - r.x)) *
            kChannels;
        std::memcpy(tile.data() + tile_offset(part.x, y, tx, ty),
                    src.data() + from,
                    static_cast<std::size_t>(part.width) * kChannels);
      }
    }
  }
}

bool TileGrid::operator==(const TileGrid& o) const {
  for (const auto& entry : tiles_) {
    const Tile* theirs = o.find(entry.first);
    if (theirs == entry.second.get()) continue;
    if (theirs ? *theirs != *entry.second : !all_zero(*entry.second)) {
      return false;
    }
  }
  for (const auto& entry : o.tiles_) {
    if (!find(entry.first) && !all_zero(*entry.second)) return false;
  }
  return true;
}

}  // namespace ps
