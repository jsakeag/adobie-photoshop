#include "layers_panel.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <nanogui/button.h>
#include <nanogui/checkbox.h>
#include <nanogui/combobox.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/slider.h>

using nanogui::Alignment;
using nanogui::BoxLayout;
using nanogui::Button;
using nanogui::Label;
using nanogui::Orientation;
using nanogui::Widget;

namespace {

constexpr int kInner = LayersPanel::kWidth - 20;

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
  // Top layer first, as in Photoshop's palette.
  for (std::size_t n = layers.size(); n-- > 0;) {
    const ps::Layer& layer = layers.at(n);
    Widget* r = row(rows_);
    auto* eye = new nanogui::CheckBox(
        r, "", [this, n](bool on) { actions_.set_visible(n, on); });
    eye->set_checked(layer.visible);
    std::string caption = layer.name.empty() ? "Layer" : layer.name;
    if (layer.mask) caption += "  [mask]";
    Button* name = button(r, "", kInner - 30, [this, n] { actions_.select(n); });
    name->set_caption(caption);
    if (n == selected) {
      name->set_background_color(nanogui::Color(60, 110, 200, 255));
    }
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
