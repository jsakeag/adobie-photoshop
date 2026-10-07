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
  pixels_ = Image(width, height, background);
}

Document::Document(Image pixels) : pixels_(std::move(pixels)) {
  require_size(pixels_.width(), pixels_.height());
}

void Document::resize_canvas(int width, int height, Anchor anchor, Color fill) {
  require_size(width, height);
  const int column = static_cast<int>(anchor) % 3;
  const int row = static_cast<int>(anchor) / 3;
  const int dx = anchor_offset(this->width(), width, column);
  const int dy = anchor_offset(this->height(), height, row);
  pixels_ = pixels_.cropped({-dx, -dy, width, height}, fill);
}

void Document::crop(Rect r) {
  r = r.intersect(bounds());
  if (r.empty()) throw std::invalid_argument("Crop rectangle misses the canvas");
  pixels_ = pixels_.cropped(r);
}

void Document::rotate(Rotation rotation) { pixels_ = pixels_.rotated(rotation); }
void Document::flip_horizontal() { pixels_ = pixels_.flipped_horizontal(); }
void Document::flip_vertical() { pixels_ = pixels_.flipped_vertical(); }

}  // namespace ps
