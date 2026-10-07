#include "ps/document.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ps {

namespace {

void require_size(int width, int height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("Document size must be at least 1x1");
  }
}

// Offset of the old content along one axis: 0 = start, 1 = middle, 2 = end.
int anchor_offset(int old_size, int new_size, int position) {
  return (new_size - old_size) * position / 2;
}

}  // namespace

Document::Document(int width, int height, Color background)
    : width_(width), height_(height) {
  require_size(width, height);
  Layer layer;
  layer.name = "Background";
  layer.pixels.fill(bounds(), background);
  layers_.insert(0, std::move(layer));
}

Document::Document(const Image& pixels)
    : width_(pixels.width()), height_(pixels.height()) {
  require_size(width_, height_);
  Layer layer;
  layer.name = "Background";
  layer.pixels.write(pixels, {0, 0});
  layers_.insert(0, std::move(layer));
}

void Document::add_layer(std::size_t index, Layer layer) {
  layer.opacity = std::clamp(layer.opacity, 0.0f, 1.0f);
  const Rect area = layer.bounds();
  layers_.insert(index, std::move(layer));
  compositor_.invalidate(area);
}

Layer Document::remove_layer(std::size_t index) {
  Layer layer = layers_.remove(index);
  compositor_.invalidate(layer.bounds());
  return layer;
}

void Document::move_layer(std::size_t from, std::size_t to) {
  layers_.move(from, to);
  compositor_.invalidate(layers_.at(to).bounds());
}

void Document::set_layer_opacity(std::size_t index, float opacity) {
  Layer& layer = layers_.at(index);
  layer.opacity = std::clamp(opacity, 0.0f, 1.0f);
  compositor_.invalidate(layer.bounds());
}

void Document::set_layer_visible(std::size_t index, bool visible) {
  Layer& layer = layers_.at(index);
  layer.visible = visible;
  compositor_.invalidate(layer.bounds());
}

void Document::set_layer_offset(std::size_t index, Point offset) {
  Layer& layer = layers_.at(index);
  compositor_.invalidate(layer.bounds());
  layer.offset = offset;
  compositor_.invalidate(layer.bounds());
}

TileGrid& Document::edit_layer_pixels(std::size_t index, Rect area) {
  Layer& layer = layers_.at(index);
  compositor_.invalidate({area.x + layer.offset.x, area.y + layer.offset.y,
                          area.width, area.height});
  return layer.pixels;
}

void Document::resize_canvas(int width, int height, Anchor anchor, Color fill) {
  require_size(width, height);
  const int column = static_cast<int>(anchor) % 3;
  const int row = static_cast<int>(anchor) / 3;
  const int dx = anchor_offset(width_, width, column);
  const int dy = anchor_offset(height_, height, row);
  // The part of the old canvas that survives, in new canvas coordinates.
  const Rect kept =
      Rect{dx, dy, width_, height_}.intersect({0, 0, width, height});
  width_ = width;
  height_ = height;

  for (std::size_t i = 0; i < layers_.size(); ++i) {
    Layer& layer = layers_.at(i);
    layer.offset = {layer.offset.x + dx, layer.offset.y + dy};
  }
  if (!layers_.empty()) {
    Layer& bottom = layers_.at(0);
    const Rect borders[] = {
        {0, 0, width, kept.y},
        {0, kept.bottom(), width, height - kept.bottom()},
        {0, kept.y, kept.x, kept.height},
        {kept.right(), kept.y, width - kept.right(), kept.height},
    };
    for (const Rect& r : borders) {
      bottom.pixels.fill({r.x - bottom.offset.x, r.y - bottom.offset.y, r.width,
                          r.height},
                         fill);
    }
  }
  compositor_.invalidate_all();
}

void Document::crop(Rect r) {
  r = r.intersect(bounds());
  if (r.empty()) throw std::invalid_argument("Crop rectangle misses the canvas");
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    Layer& layer = layers_.at(i);
    layer.offset = {layer.offset.x - r.x, layer.offset.y - r.y};
  }
  width_ = r.width;
  height_ = r.height;
  compositor_.invalidate_all();
}

void Document::remap_layers(const std::function<Image(const Image&)>& pixels,
                            const std::function<Point(Rect)>& origin) {
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    Layer& layer = layers_.at(i);
    const Rect stored = layer.pixels.bounds();
    if (stored.empty()) continue;
    const Image remapped = pixels(layer.pixels.read(stored));
    layer.offset = origin(layer.bounds());
    layer.pixels.clear();
    layer.pixels.write(remapped, {0, 0});
  }
  compositor_.invalidate_all();
}

void Document::rotate(Rotation rotation) {
  const int w = width_, h = height_;
  remap_layers(
      [&](const Image& image) { return image.rotated(rotation); },
      [&](Rect r) -> Point {
        switch (rotation) {
          case Rotation::CW90: return {h - r.bottom(), r.x};
          case Rotation::CCW90: return {r.y, w - r.right()};
          case Rotation::R180: return {w - r.right(), h - r.bottom()};
        }
        return {r.x, r.y};
      });
  if (rotation != Rotation::R180) std::swap(width_, height_);
}

void Document::flip_horizontal() {
  const int w = width_;
  remap_layers([](const Image& image) { return image.flipped_horizontal(); },
               [&](Rect r) -> Point { return {w - r.right(), r.y}; });
}

void Document::flip_vertical() {
  const int h = height_;
  remap_layers([](const Image& image) { return image.flipped_vertical(); },
               [&](Rect r) -> Point { return {r.x, h - r.bottom()}; });
}

}  // namespace ps
