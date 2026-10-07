#pragma once

#include "ps/image.h"
#include "ps/tile_grid.h"

namespace ps {

// Where the existing pixels stay put when the canvas changes size.
enum class Anchor {
  TopLeft,
  Top,
  TopRight,
  Left,
  Center,
  Right,
  BottomLeft,
  Bottom,
  BottomRight,
};

// An open image: a canvas size plus one TileGrid of pixels until layers arrive.
class Document {
 public:
  Document(int width, int height, Color background = kWhite);
  explicit Document(const Image& pixels);

  int width() const { return width_; }
  int height() const { return height_; }
  Rect bounds() const { return {0, 0, width_, height_}; }

  const TileGrid& pixels() const { return pixels_; }
  TileGrid& pixels() { return pixels_; }
  // The canvas area as one flat image.
  Image flatten() const { return pixels_.read(bounds()); }

  // Canvas Size: grows or trims around the anchor, new area gets `fill`.
  void resize_canvas(int width, int height, Anchor anchor = Anchor::Center,
                     Color fill = kWhite);

  // Crops to r clipped to the canvas. Throws if nothing is left.
  void crop(Rect r);

  void rotate(Rotation rotation);
  void flip_horizontal();
  void flip_vertical();

 private:
  // Replaces the canvas with `image`.
  void reset(const Image& image);

  int width_ = 0;
  int height_ = 0;
  TileGrid pixels_;
};

}  // namespace ps
