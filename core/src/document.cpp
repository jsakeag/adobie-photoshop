#include "ps/document.h"

#include <stdexcept>

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
  pixels_.fill(bounds(), background);
}

Document::Document(const Image& pixels) {
  require_size(pixels.width(), pixels.height());
  reset(pixels);
}

void Document::reset(const Image& image) {
  width_ = image.width();
  height_ = image.height();
  pixels_.clear();
  pixels_.write(image, {0, 0});
}

void Document::resize_canvas(int width, int height, Anchor anchor, Color fill) {
  require_size(width, height);
  const int column = static_cast<int>(anchor) % 3;
  const int row = static_cast<int>(anchor) / 3;
  const int dx = anchor_offset(this->width(), width, column);
  const int dy = anchor_offset(this->height(), height, row);
  const Rect kept = Rect{-dx, -dy, width, height}.intersect(bounds());
  const Image content = pixels_.read(kept);
  width_ = width;
  height_ = height;
  pixels_.clear();
  pixels_.fill(bounds(), fill);
  pixels_.write(content, {kept.x + dx, kept.y + dy});
}

void Document::crop(Rect r) {
  r = r.intersect(bounds());
  if (r.empty()) throw std::invalid_argument("Crop rectangle misses the canvas");
  reset(pixels_.read(r));
}

void Document::rotate(Rotation rotation) {
  reset(flatten().rotated(rotation));
}

void Document::flip_horizontal() {
  reset(flatten().flipped_horizontal());
}

void Document::flip_vertical() {
  reset(flatten().flipped_vertical());
}

}  // namespace ps
