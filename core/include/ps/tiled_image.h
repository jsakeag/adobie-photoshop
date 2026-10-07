#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "ps/geometry.h"
#include "ps/image.h"

namespace ps {

// RGBA8 image stored as a grid of fixed-size tiles.
//
// A tile that is one solid colour holds no pixel storage, and copies of a
// TiledImage share tile storage until one of them writes to a tile
// (copy-on-write). That makes blank canvases and snapshots cheap.
class TiledImage {
 public:
  static constexpr int kTileSize = 256;

  TiledImage() = default;
  TiledImage(int width, int height, Color fill = kTransparent);
  explicit TiledImage(const Image& image);

  int width() const { return width_; }
  int height() const { return height_; }
  bool empty() const { return width_ == 0 || height_ == 0; }
  Rect bounds() const { return {0, 0, width_, height_}; }
  bool contains(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
  }

  // Bounds-checked; throw std::out_of_range.
  Color pixel(int x, int y) const;
  void set_pixel(int x, int y, Color c);

  void fill(Color c);
  void fill(Rect r, Color c);  // r is clipped to the image

  // Returns the pixels under r. Parts of r outside the image get `outside`.
  Image read(Rect r, Color outside = kTransparent) const;
  Image flatten() const { return read(bounds()); }

  // Copies src with its top-left at dst, clipped. Replaces pixels, no blending.
  void write(const Image& src, Point dst);

  int tile_columns() const { return columns_; }
  int tile_rows() const { return rows_; }
  // Tiles that hold pixel storage, i.e. are not a single solid colour.
  std::size_t allocated_tiles() const;
  // Tiles whose pixel storage is the same block in both images.
  std::size_t shared_tiles(const TiledImage& other) const;

  // Compares pixel content, not how it is stored.
  bool operator==(const TiledImage& o) const;
  bool operator!=(const TiledImage& o) const { return !(*this == o); }

 private:
  struct Tile {
    Color solid = kTransparent;
    // Null while the whole tile is `solid`. Always kTileSize x kTileSize.
    std::shared_ptr<std::vector<std::uint8_t>> pixels;
  };

  // The part of the image covered by tile (tx, ty).
  Rect tile_rect(int tx, int ty) const;
  const Tile& tile(int tx, int ty) const { return tiles_[ty * columns_ + tx]; }
  // Returns storage this image owns alone, allocating or cloning as needed.
  std::uint8_t* writable(int tx, int ty);

  int width_ = 0;
  int height_ = 0;
  int columns_ = 0;
  int rows_ = 0;
  std::vector<Tile> tiles_;
};

}  // namespace ps
