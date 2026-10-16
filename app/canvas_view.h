#pragma once

#include <nanogui/imageview.h>

#include "ps/image.h"
#include "ps/viewport.h"

// Shows a document's pixels. ps::Viewport owns zoom and pan; the NanoGUI
// ImageView underneath only draws the texture and the transparency grid.
class CanvasView : public nanogui::ImageView {
 public:
  explicit CanvasView(nanogui::Widget* parent);

  // Uploads the pixels and fits them to the widget's current size.
  void show(const ps::Image& pixels);
  // Uploads new pixels for the same document. Keeps zoom and pan unless the
  // size changed, in which case it fits like show().
  void update(const ps::Image& pixels);

  const ps::Viewport& viewport() const { return viewport_; }
  // Document position under the mouse, valid while `hovering()`.
  ps::Vec2 cursor() const { return cursor_; }
  bool hovering() const { return hovering_; }

  void fit();
  void actual_size();
  void zoom_in();
  void zoom_out();

  bool mouse_drag_event(const nanogui::Vector2i& p, const nanogui::Vector2i& rel,
                        int button, int modifiers) override;
  bool mouse_motion_event(const nanogui::Vector2i& p,
                          const nanogui::Vector2i& rel, int button,
                          int modifiers) override;
  bool mouse_enter_event(const nanogui::Vector2i& p, bool enter) override;
  bool scroll_event(const nanogui::Vector2i& p, const nanogui::Vector2f& rel,
                    int flags) override;
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;
  void draw(NVGcontext* ctx) override;

 private:
  ps::Vec2 view_center() const { return {m_size.x() / 2.0, m_size.y() / 2.0}; }

  ps::Viewport viewport_;
  ps::Vec2 cursor_;
  bool hovering_ = false;
};
