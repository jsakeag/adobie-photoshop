#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

#include "ps/compositor.h"
#include "ps/image.h"
#include "ps/layer_stack.h"
#include "ps/transform.h"

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

// Everything a Document holds, as a value. Copies share tiles with the
// document, so keeping one costs only the tiles that later change.
struct DocumentState {
  int width = 0;
  int height = 0;
  LayerStack layers;
  std::optional<MaskGrid> selection;
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

  DocumentState snapshot() const {
    return {width_, height_, layers_, selection_};
  }
  // Puts back a snapshot, recompositing only the tiles that differ.
  void restore(const DocumentState& state);

  // In document coordinates; std::nullopt means nothing is selected, so
  // edits apply everywhere. Painting and fills are limited to it.
  const std::optional<MaskGrid>& selection() const { return selection_; }
  void set_selection(std::optional<MaskGrid> selection) {
    selection_ = std::move(selection);
  }

  // Inserts below the layer currently at `index`; layers().size() adds on top.
  void add_layer(std::size_t index, Layer layer);
  Layer remove_layer(std::size_t index);
  // Moves a layer so that it ends up at index `to`.
  void move_layer(std::size_t from, std::size_t to);
  void set_layer_opacity(std::size_t index, float opacity);  // clamped to 0..1
  void set_layer_visible(std::size_t index, bool visible);
  void set_layer_offset(std::size_t index, Point offset);
  void set_layer_blend(std::size_t index, BlendMode blend);
  // Adds or replaces the layer's mask; std::nullopt removes it.
  void set_layer_mask(std::size_t index, std::optional<MaskGrid> mask);
  // The layer's pixels for writing. `area` (layer coordinates) must cover
  // everything the caller is about to change.
  TileGrid& edit_layer_pixels(std::size_t index, Rect area);
  // The same for the layer's mask. Throws std::logic_error if it has none.
  MaskGrid& edit_layer_mask(std::size_t index, Rect area);

  // The layers composited over transparent, in document coordinates.
  Image render(Rect r) const { return compositor_.render(layers_, r); }
  // The canvas area as one flat image.
  Image flatten() const { return render(bounds()); }
  const Compositor& compositor() const { return compositor_; }

  // The canvas operations below also deselect, as Photoshop's Crop does.

  // Canvas Size: grows or trims around the anchor. New canvas area is filled
  // with `fill` on the bottom layer; layers keep pixels that fall outside.
  void resize_canvas(int width, int height, Anchor anchor = Anchor::Center,
                     Color fill = kWhite);

  // Crops the canvas to r clipped to it. Throws if nothing is left.
  void crop(Rect r);

  void rotate(Rotation rotation);
  void flip_horizontal();
  void flip_vertical();

  // Image Size: resamples every layer and mask to fill a width x height
  // canvas. Pixels outside the old canvas are dropped; layers end up at
  // offset (0, 0).
  void resize_image(int width, int height, Interpolation interpolation);

  // Maps one layer and its mask through `m` (document coordinates),
  // resampling with `interpolation`. Leaves the selection alone.
  void transform_layer(std::size_t index, const Matrix3& m,
                       Interpolation interpolation);

 private:
  // Rebuilds each layer and its mask from `pixels(old image)`, placed where
  // `origin` maps the old document rectangle.
  void remap_layers(const std::function<Image(const Image&)>& pixels,
                    const std::function<Point(Rect)>& origin);

  int width_ = 0;
  int height_ = 0;
  LayerStack layers_;
  std::optional<MaskGrid> selection_;
  mutable Compositor compositor_;
};

}  // namespace ps
