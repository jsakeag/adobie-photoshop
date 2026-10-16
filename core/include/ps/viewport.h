#pragma once

#include "ps/geometry.h"

namespace ps {

// Maps between document pixels and view (widget) coordinates.
// screen = doc * zoom + offset, so `offset` is where the document's
// top-left corner lands in the view.
class Viewport {
 public:
  static constexpr double kMinZoom = 1.0 / 16.0;
  static constexpr double kMaxZoom = 16.0;

  double zoom() const { return zoom_; }
  Vec2 offset() const { return offset_; }
  void set_offset(Vec2 offset) { offset_ = offset; }

  Vec2 doc_to_screen(Vec2 doc) const;
  Vec2 screen_to_doc(Vec2 screen) const;

  void pan(Vec2 screen_delta);

  // Sets the zoom (clamped) while keeping the document point under
  // `screen_anchor` fixed on screen.
  void zoom_about(Vec2 screen_anchor, double zoom);

  // Steps through the fixed zoom ratios (1:16 ... 16:1) around the anchor.
  void zoom_in(Vec2 screen_anchor);
  void zoom_out(Vec2 screen_anchor);

  // Centers the document in the view at the given zoom (clamped).
  void center(int doc_width, int doc_height, int view_width, int view_height,
              double zoom);

  // Centers the document at the largest zoom that shows all of it,
  // never above 100%.
  void fit(int doc_width, int doc_height, int view_width, int view_height);

 private:
  double zoom_ = 1.0;
  Vec2 offset_;
};

}  // namespace ps
