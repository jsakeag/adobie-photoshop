#include "tools_panel.h"

#include <cmath>
#include <string>
#include <utility>

#include <nanogui/button.h>
#include <nanogui/colorpicker.h>
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

constexpr int kInner = ToolsPanel::kWidth - 20;
const nanogui::Color kActive(60, 110, 200, 255);
const nanogui::Color kPlain(0, 0, 0, 0);

Widget* row(Widget* parent) {
  Widget* w = new Widget(parent);
  w->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 4));
  return w;
}

Button* button(Widget* parent, const std::string& caption, int width,
               std::function<void()> callback) {
  Button* b = new Button(parent, caption);
  b->set_fixed_width(width);
  b->set_callback(std::move(callback));
  return b;
}

std::string whole(float v) { return std::to_string(static_cast<int>(std::lround(v))); }

std::string percent(float v) { return whole(v * 100) + "%"; }

nanogui::Color to_ui(ps::Color c) { return nanogui::Color(c.r, c.g, c.b, 255); }

ps::Color from_ui(const nanogui::Color& c) {
  const auto byte = [](float v) {
    return static_cast<std::uint8_t>(std::lround(std::fmin(std::fmax(v, 0.f), 1.f) * 255));
  };
  return {byte(c.r()), byte(c.g()), byte(c.b()), 255};
}

}  // namespace

ToolsPanel::ToolsPanel(nanogui::Widget* parent, ToolsPanelActions actions)
    : nanogui::Window(parent, "Tools"), actions_(std::move(actions)) {
  set_fixed_width(kWidth);
  set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 5));

  Widget* grid = new Widget(this);
  grid->set_layout(new nanogui::GridLayout(Orientation::Horizontal, 2,
                                           Alignment::Fill, 0, 4));
  for (ps::ToolKind kind : ps::kToolKinds) {
    tool_buttons_.push_back(button(grid, ps::tool_name(kind), (kInner - 4) / 2,
                                   [this, kind] { actions_.set_tool(kind); }));
  }

  new Label(this, "Colour");
  color_ = new nanogui::ColorPicker(this, to_ui(state_.color));
  color_->set_final_callback(
      [this](const nanogui::Color& c) { actions_.set_color(from_ui(c)); });

  size_ = slider("Size", 1, 200, [this](float v) {
    state_.brush.diameter = v;
    size_label_->set_caption("Size: " + whole(v) + " px");
    actions_.set_brush(state_.brush);
  }, &size_label_);
  hardness_ = slider("Hardness", 0, 1, [this](float v) {
    state_.brush.hardness = v;
    hardness_label_->set_caption("Hardness: " + percent(v));
    actions_.set_brush(state_.brush);
  }, &hardness_label_);
  opacity_ = slider("Opacity", 0, 1, [this](float v) {
    state_.brush.opacity = v;
    opacity_label_->set_caption("Opacity: " + percent(v));
    actions_.set_brush(state_.brush);
  }, &opacity_label_);
  tolerance_ = slider("Tolerance", 0, 255, [this](float v) {
    state_.tolerance = static_cast<int>(std::lround(v));
    tolerance_label_->set_caption("Tolerance: " + whole(v));
    actions_.set_tolerance(state_.tolerance);
  }, &tolerance_label_);

  new Label(this, "Paint on");
  Widget* target = row(this);
  on_layer_ = button(target, "Layer", (kInner - 4) / 2,
                     [this] { actions_.set_paint_on_mask(false); });
  on_mask_ = button(target, "Mask", (kInner - 4) / 2,
                    [this] { actions_.set_paint_on_mask(true); });

  new Label(this, "Select");
  Widget* select = row(this);
  const int third = (kInner - 8) / 3;
  button(select, "All", third, [this] { actions_.select_all(); });
  button(select, "None", third, [this] { actions_.deselect(); });
  inverse_ = button(select, "Inverse", third, [this] { actions_.invert_selection(); });
  feather_radius_ = slider("Feather", 1, 50, [this](float v) {
    state_.feather_radius = v;
    feather_label_->set_caption("Feather radius: " + whole(v) + " px");
    actions_.set_feather_radius(v);
  }, &feather_label_);
  feather_ = button(this, "Feather", kInner, [this] { actions_.feather_selection(); });
}

nanogui::Slider* ToolsPanel::slider(const char* caption, float lo, float hi,
                                    std::function<void(float)> changed,
                                    nanogui::Label** label) {
  *label = new Label(this, caption);
  auto* s = new nanogui::Slider(this);
  s->set_range({lo, hi});
  s->set_callback(std::move(changed));
  return s;
}

void ToolsPanel::refresh(const ToolsPanelState& state) {
  state_ = state;
  for (std::size_t i = 0; i < ps::kToolKinds.size(); ++i) {
    tool_buttons_[i]->set_background_color(
        ps::kToolKinds[i] == state.tool ? kActive : kPlain);
  }
  color_->set_color(to_ui(state.color));

  size_->set_value(state.brush.diameter);
  size_label_->set_caption("Size: " + whole(state.brush.diameter) + " px");
  hardness_->set_value(state.brush.hardness);
  hardness_label_->set_caption("Hardness: " + percent(state.brush.hardness));
  opacity_->set_value(state.brush.opacity);
  opacity_label_->set_caption("Opacity: " + percent(state.brush.opacity));
  tolerance_->set_value(static_cast<float>(state.tolerance));
  tolerance_label_->set_caption("Tolerance: " + whole(state.tolerance));
  feather_radius_->set_value(state.feather_radius);
  feather_label_->set_caption("Feather radius: " + whole(state.feather_radius) +
                              " px");

  const bool on_mask = state.paint_on_mask && state.layer_has_mask;
  on_layer_->set_background_color(on_mask ? kPlain : kActive);
  on_mask_->set_background_color(on_mask ? kActive : kPlain);
  on_mask_->set_enabled(state.layer_has_mask);
  inverse_->set_enabled(state.has_selection);
  feather_->set_enabled(state.has_selection);
}
