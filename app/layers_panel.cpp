#include "layers_panel.h"

#include "ps/thumbnail.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <nanogui/button.h>
#include <nanogui/checkbox.h>
#include <nanogui/combobox.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/opengl.h>
#include <nanogui/popup.h>
#include <nanogui/screen.h>
#include <nanogui/slider.h>

using nanogui::Alignment;
using nanogui::BoxLayout;
using nanogui::Button;
using nanogui::Label;
using nanogui::Orientation;
using nanogui::Widget;

namespace {

constexpr int kInner = LayersPanel::kWidth - 20;
constexpr int kThumb = 32;    // thumbnail size
constexpr int kRowHeight = 38;

Widget* row(Widget* parent) {
  Widget* w = new Widget(parent);
  w->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 4));
  return w;
}

Button* button(Widget* parent, const char* caption, int width,
               std::function<void()> callback) {
  Button* b = new Button(parent, caption);
  b->set_fixed_width(width);
  b->set_callback(std::move(callback));
  return b;
}

std::string percent(float opacity) {
  return "Opacity: " + std::to_string(static_cast<int>(std::lround(opacity * 100))) +
         "%";
}

// One layer in the list: visibility checkbox, thumbnail and name. A click
// selects it; dragging it up or down moves it in the stack by whole rows.
// The row draws its thumbnail and name itself rather than holding child
// widgets, because NanoGUI sends drag events to the innermost widget under
// the pointer and the row has to receive them.
class LayerRow : public Widget {
 public:
  LayerRow(Widget* parent, std::size_t index, bool selected, std::string name,
           int thumbnail, nanogui::Vector2i thumbnail_size,
           std::function<void(std::size_t)> select,
           std::function<void(int rows)> drop)
      : Widget(parent),
        index_(index),
        selected_(selected),
        name_(std::move(name)),
        thumbnail_(thumbnail),
        thumbnail_size_(thumbnail_size),
        select_(std::move(select)),
        drop_(std::move(drop)) {
    set_fixed_height(kRowHeight);
  }

  void perform_layout(NVGcontext* ctx) override {
    Widget::perform_layout(ctx);
    for (Widget* child : m_children) {  // the checkbox, on the left
      child->set_position({4, (m_size.y() - child->height()) / 2});
    }
  }

  bool mouse_button_event(const nanogui::Vector2i& p, int button, bool down,
                          int modifiers) override {
    if (Widget::mouse_button_event(p, button, down, modifiers)) return true;
    if (button != GLFW_MOUSE_BUTTON_LEFT) return false;
    if (down) {
      pressed_ = true;
      dragged_ = 0;
      select_(index_);
    } else if (pressed_) {
      pressed_ = false;
      // The list shows the top layer first, so dragging down lowers it.
      const int rows = static_cast<int>(std::lround(float(dragged_) / kRowHeight));
      dragged_ = 0;
      if (rows != 0) drop_(-rows);
    }
    return true;
  }

  bool mouse_drag_event(const nanogui::Vector2i&, const nanogui::Vector2i& rel, int,
                        int) override {
    if (!pressed_) return false;
    dragged_ += rel.y();
    return true;
  }

  void draw(NVGcontext* ctx) override {
    nvgSave(ctx);
    if (dragged_ != 0) {
      // Follow the mouse while dragging, over the other rows.
      nvgResetScissor(ctx);
      nvgTranslate(ctx, 0, float(dragged_));
    }
    if (selected_ || dragged_ != 0) {
      nvgBeginPath(ctx);
      nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), 3);
      nvgFillColor(ctx, nvgRGBA(60, 110, 200, dragged_ != 0 ? 200 : 255));
      nvgFill(ctx);
    }
    draw_thumbnail(ctx, m_pos.x() + 32.0f, m_pos.y() + (m_size.y() - kThumb) / 2.0f);
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, float(m_theme->m_standard_font_size));
    nvgFillColor(ctx, m_theme->m_text_color);
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, m_pos.x() + 32.0f + kThumb + 8, m_pos.y() + m_size.y() / 2.0f,
            name_.c_str(), nullptr);
    Widget::draw(ctx);
    nvgRestore(ctx);
  }

 private:
  // The thumbnail on a light checkerboard, its shape fitted in the square.
  void draw_thumbnail(NVGcontext* ctx, float left, float top) {
    const nanogui::Vector2i size = thumbnail_size_;
    const float scale = float(kThumb) / std::max(size.x(), size.y());
    const float w = size.x() * scale, h = size.y() * scale;
    const float x = left + (kThumb - w) / 2, y = top + (kThumb - h) / 2;
    for (int cy = 0; cy < 4; ++cy) {
      for (int cx = 0; cx < 4; ++cx) {
        nvgBeginPath(ctx);
        nvgRect(ctx, x + cx * w / 4, y + cy * h / 4, w / 4, h / 4);
        nvgFillColor(ctx, (cx + cy) % 2 ? nvgRGB(200, 200, 200) : nvgRGB(240, 240, 240));
        nvgFill(ctx);
      }
    }
    if (thumbnail_ > 0) {
      nvgBeginPath(ctx);
      nvgRect(ctx, x, y, w, h);
      nvgFillPaint(ctx, nvgImagePattern(ctx, x, y, w, h, 0, thumbnail_, 1.0f));
      nvgFill(ctx);
    }
  }

  std::size_t index_;
  bool selected_;
  std::string name_;
  int thumbnail_;  // NanoVG image, owned by the panel
  nanogui::Vector2i thumbnail_size_;
  std::function<void(std::size_t)> select_;
  std::function<void(int)> drop_;
  bool pressed_ = false;
  int dragged_ = 0;  // vertical distance since the press, in pixels
};

}  // namespace

LayersPanel::LayersPanel(nanogui::Widget* parent, LayersPanelActions actions)
    : nanogui::Window(parent, "Layers"), actions_(std::move(actions)) {
  set_fixed_width(kWidth);
  set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 6));

  undo_ = button(this, "Undo", kInner, [this] { actions_.toggle_undo(); });

  std::vector<std::string> names;
  for (ps::BlendMode mode : ps::kBlendModes) {
    names.push_back(ps::blend_mode_name(mode));
  }
  blend_ = new nanogui::ComboBox(this, names);
  // Docked at the window's right edge: open the menu over the canvas.
  blend_->set_side(nanogui::Popup::Left);
  blend_->set_callback([this](int i) {
    actions_.set_blend(ps::kBlendModes.at(static_cast<std::size_t>(i)));
  });

  opacity_label_ = new Label(this, percent(1.0f));
  opacity_ = new nanogui::Slider(this);
  opacity_->set_callback(
      [this](float v) { opacity_label_->set_caption(percent(v)); });
  opacity_->set_final_callback([this](float v) { actions_.set_opacity(v); });

  rows_ = new Widget(this);
  rows_->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 2));

  Widget* order = row(this);
  const int quarter = (kInner - 12) / 4;
  button(order, "New", quarter, [this] { actions_.new_layer(); });
  delete_ = button(order, "Delete", quarter, [this] { actions_.delete_layer(); });
  raise_ = button(order, "Up", quarter, [this] { actions_.raise_layer(); });
  lower_ = button(order, "Down", quarter, [this] { actions_.lower_layer(); });

  button(this, "Open Image as Layer...", kInner,
         [this] { actions_.open_as_layer(); });

  new Label(this, "Layer mask");
  Widget* mask = row(this);
  const int third = (kInner - 8) / 3;
  reveal_mask_ = button(mask, "Reveal All", third, [this] { actions_.add_mask(true); });
  hide_mask_ = button(mask, "Hide All", third, [this] { actions_.add_mask(false); });
  remove_mask_ = button(mask, "Remove", third, [this] { actions_.remove_mask(); });
}

void LayersPanel::refresh(const ps::Editor& editor, std::size_t selected) {
  const ps::LayerStack& layers = editor.document().layers();
  const bool any = !layers.empty() && selected < layers.size();

  if (editor.can_undo()) {
    undo_->set_caption("Undo " + editor.undo_name());
  } else if (editor.can_redo()) {
    undo_->set_caption("Redo " + editor.redo_name());
  } else {
    undo_->set_caption("Undo");
  }
  undo_->set_enabled(editor.can_undo() || editor.can_redo());

  while (rows_->child_count() > 0) {
    rows_->remove_child_at(rows_->child_count() - 1);
  }
  // Thumbnails are NanoVG images. Free the last set here, while the
  // context is alive; any left at exit go with the context.
  NVGcontext* nvg = screen()->nvg_context();
  for (int image : thumbnails_) nvgDeleteImage(nvg, image);
  thumbnails_.clear();

  // Top layer first, as in Photoshop's palette.
  for (std::size_t n = layers.size(); n-- > 0;) {
    const ps::Layer& layer = layers.at(n);
    const std::size_t count = layers.size();
    const ps::Image thumb = ps::layer_thumbnail(editor.document(), n, kThumb);
    const int image = nvgCreateImageRGBA(nvg, thumb.width(), thumb.height(),
                                         NVG_IMAGE_NEAREST, thumb.data());
    thumbnails_.push_back(image);
    std::string caption = layer.name.empty() ? "Layer" : layer.name;
    if (layer.mask) caption += "  [mask]";
    auto* r = new LayerRow(
        rows_, n, n == selected, caption, image, {thumb.width(), thumb.height()},
        [this](std::size_t i) { actions_.select(i); },
        [this, n, count](int rows) {
          const long to = std::clamp<long>(static_cast<long>(n) + rows, 0,
                                           static_cast<long>(count) - 1);
          if (to != static_cast<long>(n)) actions_.move_layer(n, static_cast<std::size_t>(to));
        });
    auto* eye = new nanogui::CheckBox(
        r, "", [this, n](bool on) { actions_.set_visible(n, on); });
    eye->set_checked(layer.visible);
  }

  const ps::Layer* current = any ? &layers.at(selected) : nullptr;
  blend_->set_enabled(any);
  opacity_->set_enabled(any);
  if (current) {
    for (std::size_t i = 0; i < ps::kBlendModes.size(); ++i) {
      if (ps::kBlendModes[i] == current->blend) {
        blend_->set_selected_index(static_cast<int>(i));
      }
    }
    opacity_->set_value(current->opacity);
    opacity_label_->set_caption(percent(current->opacity));
  }
  delete_->set_enabled(any && layers.size() > 1);
  raise_->set_enabled(any && selected + 1 < layers.size());
  lower_->set_enabled(any && selected > 0);
  reveal_mask_->set_enabled(any);
  hide_mask_->set_enabled(any);
  remove_mask_->set_enabled(current && current->mask.has_value());
}

nanogui::Vector2i LayersPanel::row_center(std::size_t index) const {
  // Rows are listed top layer first.
  const int count = rows_->child_count();
  const int row = count - 1 - static_cast<int>(index);
  if (row < 0 || row >= count) return {0, 0};
  const Widget* w = rows_->child_at(row);
  return w->absolute_position() + w->size() / 2;
}

void LayersPanel::open_blend_menu(bool open) {
  blend_->set_pushed(open);
  blend_->popup()->set_visible(open);
}
