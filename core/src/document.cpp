#include "ps/document.h"

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

Document::Document(int width, int height, Color background) {
  require_size(width, height);
  pixels_ = TiledImage(width, height, background);
}

Document::Document(const Image& pixels) {
  require_size(pixels.width(), pixels.height());
  pixels_ = TiledImage(pixels);
}

void Document::resize_canvas(int width, int height, Anchor anchor, Color fill) {
  require_size(width, height);
  const int column = static_cast<int>(anchor) % 3;
  const int row = static_cast<int>(anchor) / 3;
  const int dx = anchor_offset(this->width(), width, column);
  const int dy = anchor_offset(this->height(), height, row);
  // Only the surviving pixels are copied, so the new area stays unallocated.
  const Rect kept = Rect{-dx, -dy, width, height}.intersect(bounds());
  TiledImage resized(width, height, fill);
  if (!kept.empty()) resized.write(pixels_.read(kept), {kept.x + dx, kept.y + dy});
  pixels_ = std::move(resized);
}

void Document::crop(Rect r) {
  r = r.intersect(bounds());
  if (r.empty()) throw std::invalid_argument("Crop rectangle misses the canvas");
  pixels_ = TiledImage(pixels_.read(r));
}

void Document::rotate(Rotation rotation) {
  pixels_ = TiledImage(flatten().rotated(rotation));
}

void Document::flip_horizontal() {
  pixels_ = TiledImage(flatten().flipped_horizontal());
}

void Document::flip_vertical() {
  pixels_ = TiledImage(flatten().flipped_vertical());
}

}  // namespace ps
