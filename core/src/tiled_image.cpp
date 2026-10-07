#include "ps/tiled_image.h"

#include <cstring>
#include <stdexcept>

namespace ps {

namespace {

constexpr int kTile = TiledImage::kTileSize;
constexpr int kChannels = Image::kChannels;
constexpr std::size_t kTileBytes =
    static_cast<std::size_t>(kTile) * kTile * kChannels;

// Byte offset of image pixel (x, y) inside its tile's storage.
std::size_t tile_offset(int x, int y) {
  return (static_cast<std::size_t>(y % kTile) * kTile + x % kTile) * kChannels;
}

void fill_pixels(std::uint8_t* p, std::size_t count, Color c) {
  const std::uint8_t px[kChannels] = {c.r, c.g, c.b, c.a};
  for (std::size_t i = 0; i < count; ++i) {
    std::memcpy(p + i * kChannels, px, kChannels);
  }
}

}  // namespace

TiledImage::TiledImage(int width, int height, Color fill_color) {
  if (width < 0 || height < 0) {
    throw std::invalid_argument("TiledImage dimensions must be non-negative");
  }
  width_ = width;
  height_ = height;
  columns_ = (width + kTile - 1) / kTile;
  rows_ = (height + kTile - 1) / kTile;
  tiles_.assign(static_cast<std::size_t>(columns_) * rows_,
                Tile{fill_color, nullptr});
}

TiledImage::TiledImage(const Image& image)
    : TiledImage(image.width(), image.height()) {
  write(image, {0, 0});
}

Rect TiledImage::tile_rect(int tx, int ty) const {
  return Rect{tx * kTile, ty * kTile, kTile, kTile}.intersect(bounds());
}

std::uint8_t* TiledImage::writable(int tx, int ty) {
  Tile& t = tiles_[ty * columns_ + tx];
  if (!t.pixels) {
    t.pixels = std::make_shared<std::vector<std::uint8_t>>(kTileBytes);
    fill_pixels(t.pixels->data(), static_cast<std::size_t>(kTile) * kTile,
                t.solid);
  } else if (t.pixels.use_count() > 1) {
    t.pixels = std::make_shared<std::vector<std::uint8_t>>(*t.pixels);
  }
  return t.pixels->data();
}

Color TiledImage::pixel(int x, int y) const {
  if (!contains(x, y)) throw std::out_of_range("TiledImage::pixel out of range");
  const Tile& t = tile(x / kTile, y / kTile);
  if (!t.pixels) return t.solid;
  const std::uint8_t* p = t.pixels->data() + tile_offset(x, y);
  return {p[0], p[1], p[2], p[3]};
}

void TiledImage::set_pixel(int x, int y, Color c) {
  if (!contains(x, y)) {
    throw std::out_of_range("TiledImage::set_pixel out of range");
  }
  fill_pixels(writable(x / kTile, y / kTile) + tile_offset(x, y), 1, c);
}

void TiledImage::fill(Color c) { fill(bounds(), c); }

void TiledImage::fill(Rect r, Color c) {
  r = r.intersect(bounds());
  if (r.empty()) return;
  for (int ty = r.y / kTile; ty <= (r.bottom() - 1) / kTile; ++ty) {
    for (int tx = r.x / kTile; tx <= (r.right() - 1) / kTile; ++tx) {
      const Rect whole = tile_rect(tx, ty);
      const Rect part = whole.intersect(r);
      if (part == whole) {
        tiles_[ty * columns_ + tx] = Tile{c, nullptr};
        continue;
      }
      std::uint8_t* data = writable(tx, ty);
      for (int y = part.y; y < part.bottom(); ++y) {
        fill_pixels(data + tile_offset(part.x, y), part.width, c);
      }
    }
  }
}

Image TiledImage::read(Rect r, Color outside) const {
  if (r.width < 0 || r.height < 0) {
    throw std::invalid_argument("TiledImage::read needs a non-negative size");
  }
  Image out(r.width, r.height, outside);
  const Rect clip = r.intersect(bounds());
  if (clip.empty()) return out;
  for (int ty = clip.y / kTile; ty <= (clip.bottom() - 1) / kTile; ++ty) {
    for (int tx = clip.x / kTile; tx <= (clip.right() - 1) / kTile; ++tx) {
      const Rect part = tile_rect(tx, ty).intersect(clip);
      const Tile& t = tile(tx, ty);
      if (!t.pixels) {
        out.fill({part.x - r.x, part.y - r.y, part.width, part.height}, t.solid);
        continue;
      }
      for (int y = part.y; y < part.bottom(); ++y) {
        const std::size_t dst =
            (static_cast<std::size_t>(y - r.y) * r.width + (part.x - r.x)) *
            kChannels;
        std::memcpy(out.data() + dst, t.pixels->data() + tile_offset(part.x, y),
                    static_cast<std::size_t>(part.width) * kChannels);
      }
    }
  }
  return out;
}

void TiledImage::write(const Image& src, Point dst) {
  const Rect clip =
      Rect{dst.x, dst.y, src.width(), src.height()}.intersect(bounds());
  if (clip.empty()) return;
  for (int ty = clip.y / kTile; ty <= (clip.bottom() - 1) / kTile; ++ty) {
    for (int tx = clip.x / kTile; tx <= (clip.right() - 1) / kTile; ++tx) {
      const Rect part = tile_rect(tx, ty).intersect(clip);
      std::uint8_t* data = writable(tx, ty);
      for (int y = part.y; y < part.bottom(); ++y) {
        const std::size_t from =
            (static_cast<std::size_t>(y - dst.y) * src.width() +
             (part.x - dst.x)) *
            kChannels;
        std::memcpy(data + tile_offset(part.x, y), src.data() + from,
                    static_cast<std::size_t>(part.width) * kChannels);
      }
    }
  }
}

std::size_t TiledImage::allocated_tiles() const {
  std::size_t count = 0;
  for (const Tile& t : tiles_) count += t.pixels ? 1 : 0;
  return count;
}

std::size_t TiledImage::shared_tiles(const TiledImage& other) const {
  if (width_ != other.width_ || height_ != other.height_) return 0;
  std::size_t count = 0;
  for (std::size_t i = 0; i < tiles_.size(); ++i) {
    count += tiles_[i].pixels && tiles_[i].pixels == other.tiles_[i].pixels;
  }
  return count;
}

bool TiledImage::operator==(const TiledImage& o) const {
  if (width_ != o.width_ || height_ != o.height_) return false;
  for (int ty = 0; ty < rows_; ++ty) {
    for (int tx = 0; tx < columns_; ++tx) {
      const Tile& a = tile(tx, ty);
      const Tile& b = o.tile(tx, ty);
      if (a.pixels && a.pixels == b.pixels) continue;
      if (!a.pixels && !b.pixels) {
        if (a.solid != b.solid) return false;
        continue;
      }
      const Rect r = tile_rect(tx, ty);
      if (read(r) != o.read(r)) return false;
    }
  }
  return true;
}

}  // namespace ps
