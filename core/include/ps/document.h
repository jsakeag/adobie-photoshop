#pragma once

#include <cstddef>
#include <functional>

#include "ps/compositor.h"
#include "ps/image.h"
#include "ps/layer_stack.h"

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

// An open image: a canvas size and a stack of layers.
//
// Layers are changed only through the methods here, so the composite cache
// always knows which tiles are stale. Layer indices run bottom to top and
// throw std::out_of_range when invalid.
class Document {
 public:
  // Starts with one "Background" layer filled with `background`.
  Document(int width, int height, Color background = kWhite);
  // Starts with `pixels` as the one "Background" layer.
  explicit Document(const Image& pixels);

  int width() const { return width_; }
  int height() const { return height_; }
  Rect bounds() const { return {0, 0, width_, height_}; }

  const LayerStack& layers() const { return layers_; }

  // Inserts below the layer currently at `index`; layers().size() adds on top.
  void add_layer(std::size_t index, Layer layer);
  Layer remove_layer(std::size_t index);
  // Moves a layer so that it ends up at index `to`.
  void move_layer(std::size_t from, std::size_t to);
  void set_layer_opacity(std::size_t index, float opacity);  // clamped to 0..1
  void set_layer_visible(std::size_t index, bool visible);
  void set_layer_offset(std::size_t index, Point offset);
  // The layer's pixels for writing. `area` (layer coordinates) must cover
  // everything the caller is about to change.
  TileGrid& edit_layer_pixels(std::size_t index, Rect area);

  // The layers composited over transparent, in document coordinates.
  Image render(Rect r) const { return compositor_.render(layers_, r); }
  // The canvas area as one flat image.
  Image flatten() const { return render(bounds()); }
  const Compositor& compositor() const { return compositor_; }

  // Canvas Size: grows or trims around the anchor. New canvas area is filled
  // with `fill` on the bottom layer; layers keep pixels that fall outside.
  void resize_canvas(int width, int height, Anchor anchor = Anchor::Center,
                     Color fill = kWhite);

  // Crops the canvas to r clipped to it. Throws if nothing is left.
  void crop(Rect r);

  void rotate(Rotation rotation);
  void flip_horizontal();
  void flip_vertical();

 private:
  // Rebuilds each layer from `pixels(old image)`, placed where `origin`
  // maps the layer's old document rectangle.
  void remap_layers(const std::function<Image(const Image&)>& pixels,
                    const std::function<Point(Rect)>& origin);

  int width_ = 0;
  int height_ = 0;
  LayerStack layers_;
  mutable Compositor compositor_;
};

}  // namespace ps
