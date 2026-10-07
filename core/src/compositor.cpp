#include "ps/compositor.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "ps/blend.h"

namespace ps {

namespace {

constexpr int kPixelsPerTile = kTileSize * kTileSize;

}  // namespace

Image Compositor::render(const LayerStack& stack, Rect r) {
  if (r.width < 0 || r.height < 0) {
    throw std::invalid_argument("Compositor::render needs a non-negative size");
  }
  if (r.empty()) return Image(r.width, r.height);
  for (int ty = tile_index(r.y); ty <= tile_index(r.bottom() - 1); ++ty) {
    for (int tx = tile_index(r.x); tx <= tile_index(r.right() - 1); ++tx) {
      if (!valid_.count({tx, ty})) composite_tile(stack, {tx, ty});
    }
  }
  return cache_.read(r);
}

void Compositor::invalidate(Rect r) {
  if (r.empty() || valid_.empty()) return;
  const int tx0 = tile_index(r.x), tx1 = tile_index(r.right() - 1);
  const int ty0 = tile_index(r.y), ty1 = tile_index(r.bottom() - 1);
  // Walk whichever is smaller: the tiles under r or the cached tiles.
  const long long covered =
      static_cast<long long>(tx1 - tx0 + 1) * (ty1 - ty0 + 1);
  if (covered <= static_cast<long long>(valid_.size())) {
    for (int ty = ty0; ty <= ty1; ++ty) {
      for (int tx = tx0; tx <= tx1; ++tx) valid_.erase({tx, ty});
    }
    return;
  }
  for (auto it = valid_.begin(); it != valid_.end();) {
    const bool inside =
        it->x >= tx0 && it->x <= tx1 && it->y >= ty0 && it->y <= ty1;
    it = inside ? valid_.erase(it) : std::next(it);
  }
}

void Compositor::invalidate_all() {
  valid_.clear();
  cache_.clear();
}

void Compositor::composite_tile(const LayerStack& stack, TileCoord coord) {
  const Rect area = tile_rect(coord);
  std::vector<Premul> result(kPixelsPerTile);
  bool drawn = false;

  for (const Layer& layer : stack) {
    if (!layer.visible || layer.opacity <= 0.0f) continue;
    const Rect source{area.x - layer.offset.x, area.y - layer.offset.y,
                      kTileSize, kTileSize};
    if (!layer.pixels.any_tile_in(source)) continue;

    const Image pixels = layer.pixels.read(source);
    const float opacity = std::min(layer.opacity, 1.0f);
    const std::uint8_t* p = pixels.data();
    for (int i = 0; i < kPixelsPerTile; ++i, p += Image::kChannels) {
      if (p[3] == 0) continue;
      Premul src = premultiply({p[0], p[1], p[2], p[3]});
      src.r *= opacity;
      src.g *= opacity;
      src.b *= opacity;
      src.a *= opacity;
      result[i] = blend(layer.blend, result[i], src);
    }
    drawn = true;
  }

  if (drawn) {
    Image out(kTileSize, kTileSize);
    std::uint8_t* p = out.data();
    for (int i = 0; i < kPixelsPerTile; ++i, p += Image::kChannels) {
      const Color c = unpremultiply(result[i]);
      p[0] = c.r;
      p[1] = c.g;
      p[2] = c.b;
      p[3] = c.a;
    }
    cache_.write(out, {area.x, area.y});
  } else {
    cache_.fill(area, kTransparent);  // drops any stale tile
  }
  valid_.insert(coord);
  ++tiles_composited_;
}

}  // namespace ps
