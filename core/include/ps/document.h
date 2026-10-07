#pragma once

#include "ps/image.h"

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

// An open image. Holds a single pixel buffer until layers arrive.
class Document {
 public:
  Document(int width, int height, Color background = kWhite);
  explicit Document(Image pixels);

  int width() const { return pixels_.width(); }
  int height() const { return pixels_.height(); }
  Rect bounds() const { return pixels_.bounds(); }

  const Image& pixels() const { return pixels_; }
  Image& pixels() { return pixels_; }

  // Canvas Size: grows or trims around the anchor, new area gets `fill`.
  void resize_canvas(int width, int height, Anchor anchor = Anchor::Center,
                     Color fill = kWhite);

  // Crops to r clipped to the canvas. Throws if nothing is left.
  void crop(Rect r);

  void rotate(Rotation rotation);
  void flip_horizontal();
  void flip_vertical();

 private:
  Image pixels_;
};

}  // namespace ps
