#include "ps/viewport.h"

#include <algorithm>
#include <array>

namespace ps {

namespace {

constexpr std::array<double, 18> kZoomSteps = {
    1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 6, 1.0 / 4, 1.0 / 3,
    1.0 / 2,  2.0 / 3,  1.0,     2.0,     3.0,     4.0,
    5.0,      6.0,      7.0,     8.0,     12.0,    16.0};

// Guards against a zoom that is a step value give or take rounding.
constexpr double kEpsilon = 1e-9;

}  // namespace

Vec2 Viewport::doc_to_screen(Vec2 doc) const {
  return {doc.x * zoom_ + offset_.x, doc.y * zoom_ + offset_.y};
}

Vec2 Viewport::screen_to_doc(Vec2 screen) const {
  return {(screen.x - offset_.x) / zoom_, (screen.y - offset_.y) / zoom_};
}

void Viewport::pan(Vec2 screen_delta) {
  offset_.x += screen_delta.x;
  offset_.y += screen_delta.y;
}

void Viewport::zoom_about(Vec2 screen_anchor, double zoom) {
  const Vec2 doc = screen_to_doc(screen_anchor);
  zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
  offset_ = {screen_anchor.x - doc.x * zoom_, screen_anchor.y - doc.y * zoom_};
}

void Viewport::zoom_in(Vec2 screen_anchor) {
  for (double step : kZoomSteps) {
    if (step > zoom_ * (1.0 + kEpsilon)) {
      zoom_about(screen_anchor, step);
      return;
    }
  }
}

void Viewport::zoom_out(Vec2 screen_anchor) {
  for (auto it = kZoomSteps.rbegin(); it != kZoomSteps.rend(); ++it) {
    if (*it < zoom_ * (1.0 - kEpsilon)) {
      zoom_about(screen_anchor, *it);
      return;
    }
  }
}

void Viewport::center(int doc_width, int doc_height, int view_width,
                      int view_height, double zoom) {
  zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
  offset_ = {(view_width - doc_width * zoom_) / 2.0,
             (view_height - doc_height * zoom_) / 2.0};
}

void Viewport::fit(int doc_width, int doc_height, int view_width,
                   int view_height) {
  double zoom = 1.0;
  if (doc_width > 0 && doc_height > 0) {
    zoom = std::min({1.0, static_cast<double>(view_width) / doc_width,
                     static_cast<double>(view_height) / doc_height});
  }
  center(doc_width, doc_height, view_width, view_height, zoom);
}

}  // namespace ps
