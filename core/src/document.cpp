#include "ps/document.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

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

Rect offset_by(Rect r, Point p) {
  return {r.x + p.x, r.y + p.y, r.width, r.height};
}

// Mask values carried in an Image so they can share its flips and rotations.
Image mask_as_image(const std::vector<std::uint8_t>& values, Rect r) {
  Image image(r.width, r.height);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint8_t* p = image.data() + i * Image::kChannels;
    p[0] = p[1] = p[2] = values[i];
    p[3] = 255;  // opaque, so resampling treats it as plain grey
  }
  return image;
}

Rect grown(Rect r, int by) {
  return {r.x - by, r.y - by, r.width + 2 * by, r.height + 2 * by};
}

std::vector<std::uint8_t> image_as_mask(const Image& image) {
  std::vector<std::uint8_t> values(static_cast<std::size_t>(image.width()) *
                                   image.height());
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = image.data()[i * Image::kChannels];
  }
  return values;
}

// Whether two layers differ only in pixel and mask content, if at all.
bool same_placement(const Layer& a, const Layer& b) {
  if (a.offset.x != b.offset.x || a.offset.y != b.offset.y ||
      a.opacity != b.opacity || a.visible != b.visible || a.blend != b.blend ||
      a.mask.has_value() != b.mask.has_value()) {
    return false;
  }
  return !a.mask || a.mask->background() == b.mask->background();
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

void Document::restore(const DocumentState& state) {
  require_size(state.width, state.height);
  if (state.width != width_ || state.height != height_ ||
      state.layers.size() != layers_.size()) {
    compositor_.invalidate_all();
  } else {
    for (std::size_t i = 0; i < layers_.size(); ++i) {
      const Layer& now = layers_.at(i);
      const Layer& then = state.layers.at(i);
      if (!same_placement(now, then)) {
        compositor_.invalidate(now.bounds());
        compositor_.invalidate(then.bounds());
        continue;
      }
      for (TileCoord c : now.pixels.changed_tiles(then.pixels)) {
        compositor_.invalidate(offset_by(tile_rect(c), now.offset));
      }
      if (!now.mask) continue;
      for (TileCoord c : now.mask->changed_tiles(*then.mask)) {
        compositor_.invalidate(offset_by(tile_rect(c), now.offset));
      }
    }
  }
  width_ = state.width;
  height_ = state.height;
  layers_ = state.layers;
  selection_ = state.selection;
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

void Document::set_layer_blend(std::size_t index, BlendMode blend) {
  Layer& layer = layers_.at(index);
  layer.blend = blend;
  compositor_.invalidate(layer.bounds());
}

void Document::set_layer_mask(std::size_t index, std::optional<MaskGrid> mask) {
  Layer& layer = layers_.at(index);
  layer.mask = std::move(mask);
  compositor_.invalidate(layer.bounds());
}

TileGrid& Document::edit_layer_pixels(std::size_t index, Rect area) {
  Layer& layer = layers_.at(index);
  compositor_.invalidate(offset_by(area, layer.offset));
  return layer.pixels;
}

MaskGrid& Document::edit_layer_mask(std::size_t index, Rect area) {
  Layer& layer = layers_.at(index);
  if (!layer.mask) throw std::logic_error("Layer has no mask to edit");
  compositor_.invalidate(offset_by(area, layer.offset));
  return *layer.mask;
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
  selection_.reset();
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
  selection_.reset();
  compositor_.invalidate_all();
}

void Document::remap_layers(const std::function<Image(const Image&)>& pixels,
                            const std::function<Point(Rect)>& origin) {
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    Layer& layer = layers_.at(i);
    const Point old_offset = layer.offset;
    const Rect stored = layer.pixels.bounds();
    const Image remapped = pixels(layer.pixels.read(stored));
    // The rebuilt pixels start at the layer's own (0, 0).
    layer.offset = origin(offset_by(stored, old_offset));
    layer.pixels.clear();
    layer.pixels.write(remapped, {0, 0});

    if (!layer.mask || layer.mask->empty()) continue;
    const Rect masked = layer.mask->bounds();
    const Image mask =
        pixels(mask_as_image(layer.mask->read(masked), masked));
    const Point at = origin(offset_by(masked, old_offset));
    layer.mask->clear();
    layer.mask->write(image_as_mask(mask),
                      {at.x - layer.offset.x, at.y - layer.offset.y,
                       mask.width(), mask.height()});
  }
  selection_.reset();
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

void Document::resize_image(int width, int height, Interpolation interpolation) {
  require_size(width, height);
  const Matrix3 m = Matrix3::scale(double(width) / width_, double(height) / height_);
  const Rect canvas = bounds();
  const Rect target{0, 0, width, height};
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    Layer& layer = layers_.at(i);
    // The old canvas is the source, so its edges repeat rather than fade.
    const Rect source = offset_by(canvas, {-layer.offset.x, -layer.offset.y});
    const Rect shown = layer.bounds().intersect(canvas);
    TileGrid pixels;
    if (!shown.empty()) {
      const Rect area = transformed_bounds(m, shown).intersect(target);
      pixels.write(transform_image(layer.pixels.read(source), {0, 0}, m, area,
                                   interpolation),
                   {area.x, area.y});
    }
    layer.pixels = std::move(pixels);
    if (layer.mask && !layer.mask->empty()) {
      const Image values = mask_as_image(layer.mask->read(source), canvas);
      MaskGrid mask(layer.mask->background());
      mask.write(image_as_mask(transform_image(values, {0, 0}, m, target, interpolation)),
                 target);
      layer.mask = std::move(mask);
    }
    layer.offset = {0, 0};
  }
  width_ = width;
  height_ = height;
  selection_.reset();
  compositor_.invalidate_all();
}

void Document::transform_layer(std::size_t index, const Matrix3& m,
                               Interpolation interpolation) {
  Layer& layer = layers_.at(index);
  const Point to_layer{-layer.offset.x, -layer.offset.y};
  const Rect old_bounds = layer.bounds();

  if (!layer.pixels.empty()) {
    // Two pixels of transparent margin, so edges fade instead of repeating.
    const Rect source = grown(layer.bounds(), 2);
    const Rect target = transformed_bounds(m, source);
    const Image out = transform_image(layer.pixels.read(offset_by(source, to_layer)),
                                      {source.x, source.y}, m, target, interpolation);
    layer.pixels.clear();
    layer.pixels.write(out, {target.x + to_layer.x, target.y + to_layer.y});
  }
  if (layer.mask && !layer.mask->empty()) {
    // The margin reads the mask's background, which beyond it stays as is.
    const Rect stored = grown(layer.mask->bounds(), 2);
    const Rect source = offset_by(stored, layer.offset);
    const Rect target = transformed_bounds(m, source);
    const Image out = transform_image(mask_as_image(layer.mask->read(stored), stored),
                                      {source.x, source.y}, m, target, interpolation);
    layer.mask->clear();
    layer.mask->write(image_as_mask(out), offset_by(target, to_layer));
  }
  compositor_.invalidate(old_bounds);
  compositor_.invalidate(layer.bounds());
}

}  // namespace ps
