#include "canvas_view.h"

#include <cmath>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanogui/texture.h>

using nanogui::Texture;
using nanogui::Vector2f;
using nanogui::Vector2i;

CanvasView::CanvasView(nanogui::Widget* parent) : nanogui::ImageView(parent) {
  set_draw_border(false);
}

void CanvasView::show(const ps::Image& pixels) {
  nanogui::ref<Texture> texture = new Texture(
      Texture::PixelFormat::RGBA, Texture::ComponentFormat::UInt8,
      Vector2i(pixels.width(), pixels.height()),
      Texture::InterpolationMode::Trilinear, Texture::InterpolationMode::Nearest);
  texture->upload(pixels.data());
  set_image(texture);
  fit();
}

void CanvasView::update(const ps::Image& pixels) {
  if (!m_image || m_image->size() != Vector2i(pixels.width(), pixels.height())) {
    show(pixels);
    return;
  }
  m_image->upload(pixels.data());
}

void CanvasView::fit() {
  if (!m_image) return;
  viewport_.fit(m_image->size().x(), m_image->size().y(), m_size.x(), m_size.y());
}

void CanvasView::actual_size() {
  if (!m_image) return;
  viewport_.center(m_image->size().x(), m_image->size().y(), m_size.x(),
                   m_size.y(), 1.0);
}

void CanvasView::zoom_in() { viewport_.zoom_in(view_center()); }
void CanvasView::zoom_out() { viewport_.zoom_out(view_center()); }

ps::Vec2 CanvasView::to_document(const Vector2i& p) const {
  const Vector2i local = p - m_pos;
  return viewport_.screen_to_doc({double(local.x()), double(local.y())});
}

bool CanvasView::mouse_button_event(const Vector2i& p, int button, bool down,
                                    int modifiers) {
  nanogui::ImageView::mouse_button_event(p, button, down, modifiers);
  if (button != GLFW_MOUSE_BUTTON_LEFT || !pointer_) return true;
  if (down && !space_held_) {
    gesture_ = true;
    pointer_(PointerPhase::Press, to_document(p), modifiers);
  } else if (!down && gesture_) {
    gesture_ = false;
    pointer_(PointerPhase::Release, to_document(p), modifiers);
  }
  return true;
}

bool CanvasView::mouse_drag_event(const Vector2i& p, const Vector2i& rel,
                                  int /* button */, int modifiers) {
  cursor_ = to_document(p);
  if (gesture_) {
    pointer_(PointerPhase::Drag, cursor_, modifiers);
    return true;
  }
  viewport_.pan({static_cast<double>(rel.x()), static_cast<double>(rel.y())});
  return true;
}

bool CanvasView::mouse_motion_event(const Vector2i& p, const Vector2i& /* rel */,
                                    int /* button */, int /* modifiers */) {
  const Vector2i local = p - m_pos;
  cursor_ = viewport_.screen_to_doc({double(local.x()), double(local.y())});
  hovering_ = true;
  return true;
}

bool CanvasView::mouse_enter_event(const Vector2i& p, bool enter) {
  hovering_ = enter;
  return nanogui::ImageView::mouse_enter_event(p, enter);
}

bool CanvasView::scroll_event(const Vector2i& p, const Vector2f& rel,
                              int /* flags */) {
  const Vector2i local = p - m_pos;
  viewport_.zoom_about({double(local.x()), double(local.y())},
                       viewport_.zoom() * std::pow(1.1, rel.y()));
  return true;
}

bool CanvasView::keyboard_event(int, int, int, int) {
  return false;  // shortcuts are handled by the screen
}

void CanvasView::draw(NVGcontext* ctx) {
  // ImageView works in framebuffer pixels, the viewport in widget units.
  const float ratio = screen()->pixel_ratio();
  set_scale(static_cast<float>(viewport_.zoom()) * ratio);
  set_offset(Vector2f(static_cast<float>(viewport_.offset().x) * ratio,
                      static_cast<float>(viewport_.offset().y) * ratio));

  nanogui::ImageView::draw(ctx);

  // ImageView snaps the offset to whole pixels and keeps the image from
  // leaving the widget; adopt its result so both agree.
  viewport_.set_offset({offset().x() / ratio, offset().y() / ratio});
}
