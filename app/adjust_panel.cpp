#include "adjust_panel.h"

#include <cmath>
#include <cstdio>
#include <utility>

#include <nanogui/button.h>
#include <nanogui/combobox.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/slider.h>

using nanogui::Alignment;
using nanogui::BoxLayout;
using nanogui::Orientation;

namespace {

constexpr int kInner = AdjustPanel::kWidth - 20;

// A fresh adjustment of each kind, in the menu's order.
ps::Adjustment default_of(std::size_t kind) {
  switch (kind) {
    case 0: return ps::Levels{};
    case 1: return ps::Curves{{{0, 0}, {64, 64}, {128, 128}, {192, 192}, {255, 255}}};
    case 2: return ps::BrightnessContrast{};
    case 3: return ps::HueSaturation{};
    default: return ps::Invert{};
  }
}

int whole(float v) { return static_cast<int>(std::lround(v)); }

// Accessors for one int field of one adjustment type.
template <typename T>
std::function<float(const ps::Adjustment&)> get(int T::*field) {
  return [field](const ps::Adjustment& a) { return float(std::get<T>(a).*field); };
}
template <typename T>
std::function<void(ps::Adjustment&, float)> set(int T::*field) {
  return [field](ps::Adjustment& a, float v) { std::get<T>(a).*field = whole(v); };
}

// Curves in the palette: the outputs at inputs 64, 128 and 192.
std::function<float(const ps::Adjustment&)> curve_get(std::size_t i) {
  return [i](const ps::Adjustment& a) {
    return float(std::get<ps::Curves>(a).points.at(i).y);
  };
}
std::function<void(ps::Adjustment&, float)> curve_set(std::size_t i) {
  return [i](ps::Adjustment& a, float v) {
    std::get<ps::Curves>(a).points.at(i).y = whole(v);
  };
}

}  // namespace

AdjustPanel::AdjustPanel(nanogui::Widget* parent, AdjustPanelActions actions)
    : nanogui::Window(parent, "Adjustments"),
      actions_(std::move(actions)),
      adjustment_(default_of(0)) {
  set_fixed_width(kWidth);
  set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 5));
  kinds_ = new nanogui::ComboBox(
      this, {"Levels", "Curves", "Brightness/Contrast", "Hue/Saturation", "Invert"});
  kinds_->set_callback([this](int i) {
    actions_.preview(default_of(static_cast<std::size_t>(i)));
  });
  controls_ = new nanogui::Widget(this);
  controls_->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 5));

  nanogui::Widget* buttons = new nanogui::Widget(this);
  buttons->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 4));
  auto* apply = new nanogui::Button(buttons, "Apply");
  apply->set_fixed_width((kInner - 4) / 2);
  apply->set_callback([this] { actions_.apply(); });
  auto* cancel = new nanogui::Button(buttons, "Cancel");
  cancel->set_fixed_width((kInner - 4) / 2);
  cancel->set_callback([this] { actions_.cancel(); });
}

void AdjustPanel::show(const ps::Adjustment& adjustment) {
  adjustment_ = adjustment;
  if (adjustment.index() != kind_) {
    kind_ = adjustment.index();
    kinds_->set_selected_index(static_cast<int>(kind_));
    rebuild();
  }
  for (Control& c : sliders_) c.slider->set_value(c.get(adjustment_));
  update_labels();
}

void AdjustPanel::rebuild() {
  while (controls_->child_count() > 0) {
    controls_->remove_child_at(controls_->child_count() - 1);
  }
  sliders_.clear();
  switch (kind_) {
    case 0:
      sliders_ = {
          {"Input black", 0, 254, get(&ps::Levels::input_black), set(&ps::Levels::input_black)},
          {"Input white", 1, 255, get(&ps::Levels::input_white), set(&ps::Levels::input_white)},
          {"Gamma", 0.1f, 4.0f,
           [](const ps::Adjustment& a) { return std::get<ps::Levels>(a).gamma; },
           [](ps::Adjustment& a, float v) { std::get<ps::Levels>(a).gamma = v; }},
          {"Output black", 0, 255, get(&ps::Levels::output_black), set(&ps::Levels::output_black)},
          {"Output white", 0, 255, get(&ps::Levels::output_white), set(&ps::Levels::output_white)},
      };
      break;
    case 1:
      sliders_ = {
          {"Shadows (64)", 0, 255, curve_get(1), curve_set(1)},
          {"Midtones (128)", 0, 255, curve_get(2), curve_set(2)},
          {"Highlights (192)", 0, 255, curve_get(3), curve_set(3)},
      };
      break;
    case 2:
      sliders_ = {
          {"Brightness", -100, 100, get(&ps::BrightnessContrast::brightness),
           set(&ps::BrightnessContrast::brightness)},
          {"Contrast", -100, 100, get(&ps::BrightnessContrast::contrast),
           set(&ps::BrightnessContrast::contrast)},
      };
      break;
    case 3:
      sliders_ = {
          {"Hue", -180, 180, get(&ps::HueSaturation::hue), set(&ps::HueSaturation::hue)},
          {"Saturation", -100, 100, get(&ps::HueSaturation::saturation),
           set(&ps::HueSaturation::saturation)},
          {"Lightness", -100, 100, get(&ps::HueSaturation::lightness),
           set(&ps::HueSaturation::lightness)},
      };
      break;
    default:
      new nanogui::Label(controls_, "No settings: Apply inverts.");
      break;
  }
  for (std::size_t i = 0; i < sliders_.size(); ++i) {
    Control& c = sliders_[i];
    c.label = new nanogui::Label(controls_, c.caption);
    c.slider = new nanogui::Slider(controls_);
    c.slider->set_range({c.lo, c.hi});
    c.slider->set_callback([this, i](float v) {
      sliders_[i].set(adjustment_, v);
      update_labels();
      actions_.preview(adjustment_);
    });
  }
}

void AdjustPanel::update_labels() {
  for (Control& c : sliders_) {
    const float v = c.get(adjustment_);
    char text[64];
    if (c.hi - c.lo <= 10) {
      std::snprintf(text, sizeof(text), "%s: %.2f", c.caption.c_str(), v);
    } else {
      std::snprintf(text, sizeof(text), "%s: %d", c.caption.c_str(), whole(v));
    }
    c.label->set_caption(text);
  }
}
