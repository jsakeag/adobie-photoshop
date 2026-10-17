#include "effects_panel.h"

#include <cmath>
#include <cstdio>
#include <utility>

#include <nanogui/button.h>
#include <nanogui/combobox.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/popup.h>
#include <nanogui/slider.h>

using nanogui::Alignment;
using nanogui::BoxLayout;
using nanogui::Orientation;

namespace {

constexpr int kInner = EffectsPanel::kWidth - 20;

int whole(float v) { return static_cast<int>(std::lround(v)); }

// Accessors for one field of one effect type.
template <typename T, typename F>
std::function<float(const Effect&)> get(F T::*field) {
  return [field](const Effect& e) { return float(std::get<T>(e).*field); };
}
template <typename T, typename F>
std::function<void(Effect&, float)> set(F T::*field) {
  return [field](Effect& e, float v) {
    if constexpr (std::is_integral_v<F>) {
      std::get<T>(e).*field = static_cast<F>(whole(v));
    } else {
      std::get<T>(e).*field = v;
    }
  };
}

// Curves in the palette: the outputs at inputs 64, 128 and 192.
std::function<float(const Effect&)> curve_get(std::size_t i) {
  return [i](const Effect& e) { return float(std::get<ps::Curves>(e).points.at(i).y); };
}
std::function<void(Effect&, float)> curve_set(std::size_t i) {
  return [i](Effect& e, float v) { std::get<ps::Curves>(e).points.at(i).y = whole(v); };
}

}  // namespace

EffectsPanel::EffectsPanel(nanogui::Widget* parent, EffectsPanelActions actions)
    : nanogui::Window(parent, "Image"),
      actions_(std::move(actions)),
      effect_(default_effect(0)) {
  set_fixed_width(kWidth);
  set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 5));
  for (const EffectMenu& m : kEffectMenus) {
    std::vector<std::string> items{m.placeholder};
    for (std::size_t k = m.first; k < m.first + m.count; ++k) items.push_back(effect_name(k));
    auto* menu = new nanogui::ComboBox(this, items);
    // The palette is docked at the window's right edge: open the menus over
    // the canvas, not off-screen.
    menu->set_side(nanogui::Popup::Left);
    const std::size_t first = m.first;
    menu->set_callback([this, first](int i) {
      if (i > 0) actions_.preview(default_effect(first + static_cast<std::size_t>(i - 1)));
    });
    menus_.push_back(menu);
  }
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

void EffectsPanel::show(const Effect& effect) {
  effect_ = effect;
  if (effect.index() != kind_) {
    kind_ = effect.index();
    for (std::size_t i = 0; i < kEffectMenus.size(); ++i) {
      const EffectMenu& m = kEffectMenus[i];
      const bool here = kind_ >= m.first && kind_ < m.first + m.count;
      menus_[i]->set_selected_index(here ? static_cast<int>(kind_ - m.first) + 1 : 0);
    }
    rebuild();
  }
  for (Control& c : sliders_) c.slider->set_value(c.get(effect_));
  update_labels();
}

void EffectsPanel::open_menu(std::size_t index, bool open) {
  nanogui::ComboBox* menu = menus_.at(index);
  menu->set_pushed(open);
  menu->popup()->set_visible(open);
}

void EffectsPanel::rebuild() {
  while (controls_->child_count() > 0) {
    controls_->remove_child_at(controls_->child_count() - 1);
  }
  using namespace ps;
  switch (kind_) {
    case 0:
      sliders_ = {
          {"Input black", 0, 254, 0, get(&Levels::input_black), set(&Levels::input_black)},
          {"Input white", 1, 255, 0, get(&Levels::input_white), set(&Levels::input_white)},
          {"Gamma", 0.1f, 4.0f, 2, get(&Levels::gamma), set(&Levels::gamma)},
          {"Output black", 0, 255, 0, get(&Levels::output_black), set(&Levels::output_black)},
          {"Output white", 0, 255, 0, get(&Levels::output_white), set(&Levels::output_white)},
      };
      break;
    case 1:
      sliders_ = {
          {"Shadows (64)", 0, 255, 0, curve_get(1), curve_set(1)},
          {"Midtones (128)", 0, 255, 0, curve_get(2), curve_set(2)},
          {"Highlights (192)", 0, 255, 0, curve_get(3), curve_set(3)},
      };
      break;
    case 2:
      sliders_ = {
          {"Brightness", -100, 100, 0, get(&BrightnessContrast::brightness),
           set(&BrightnessContrast::brightness)},
          {"Contrast", -100, 100, 0, get(&BrightnessContrast::contrast),
           set(&BrightnessContrast::contrast)},
      };
      break;
    case 3:
      sliders_ = {
          {"Hue", -180, 180, 0, get(&HueSaturation::hue), set(&HueSaturation::hue)},
          {"Saturation", -100, 100, 0, get(&HueSaturation::saturation),
           set(&HueSaturation::saturation)},
          {"Lightness", -100, 100, 0, get(&HueSaturation::lightness),
           set(&HueSaturation::lightness)},
      };
      break;
    case 5:
      sliders_ = {{"Radius (px)", 0, 30, 1, get(&GaussianBlur::radius),
                   set(&GaussianBlur::radius)}};
      break;
    case 6:
      sliders_ = {
          {"Amount", 0, 5, 2, get(&UnsharpMask::amount), set(&UnsharpMask::amount)},
          {"Radius (px)", 0.1f, 20, 1, get(&UnsharpMask::radius), set(&UnsharpMask::radius)},
          {"Threshold", 0, 255, 0, get(&UnsharpMask::threshold), set(&UnsharpMask::threshold)},
      };
      break;
    case 7:
      sliders_ = {{"Amount", 0, 255, 0, get(&AddNoise::amount), set(&AddNoise::amount)}};
      break;
    case 8:
      sliders_ = {
          {"Angle", -90, 90, 0, get(&MotionBlur::angle), set(&MotionBlur::angle)},
          {"Distance (px)", 1, 100, 0, get(&MotionBlur::distance), set(&MotionBlur::distance)},
      };
      break;
    case 9:
      sliders_ = {{"Size (%)", 10, 400, 0, get(&ImageSizeEffect::percent),
                   set(&ImageSizeEffect::percent)}};
      break;
    case 10: {
      static const char* const anchors[9] = {"Top left", "Top",    "Top right",
                                             "Left",     "Centre", "Right",
                                             "Bottom left", "Bottom", "Bottom right"};
      sliders_ = {
          {"Width (%)", 10, 400, 0, get(&CanvasSizeEffect::width_percent),
           set(&CanvasSizeEffect::width_percent)},
          {"Height (%)", 10, 400, 0, get(&CanvasSizeEffect::height_percent),
           set(&CanvasSizeEffect::height_percent)},
          {"Anchor", 0, 8, 0, get(&CanvasSizeEffect::anchor), set(&CanvasSizeEffect::anchor),
           [](float v) { return std::string("Anchor: ") + anchors[whole(v)]; }},
      };
      break;
    }
    case 11:
      sliders_ = {
          {"Width (%)", 10, 400, 0, get(&LayerTransformEffect::scale_x),
           set(&LayerTransformEffect::scale_x)},
          {"Height (%)", 10, 400, 0, get(&LayerTransformEffect::scale_y),
           set(&LayerTransformEffect::scale_y)},
          {"Angle", -180, 180, 0, get(&LayerTransformEffect::angle),
           set(&LayerTransformEffect::angle)},
      };
      break;
    default:
      sliders_.clear();
      new nanogui::Label(controls_, "No settings: Apply inverts.");
      break;
  }
  for (std::size_t i = 0; i < sliders_.size(); ++i) {
    Control& c = sliders_[i];
    c.label = new nanogui::Label(controls_, c.caption);
    c.slider = new nanogui::Slider(controls_);
    c.slider->set_range({c.lo, c.hi});
    c.slider->set_callback([this, i](float v) {
      sliders_[i].set(effect_, v);
      update_labels();
      actions_.preview(effect_);
    });
  }
}

void EffectsPanel::update_labels() {
  for (Control& c : sliders_) {
    if (c.format) {
      c.label->set_caption(c.format(c.get(effect_)));
      continue;
    }
    char text[64];
    std::snprintf(text, sizeof(text), "%s: %.*f", c.caption.c_str(), c.decimals,
                  static_cast<double>(c.get(effect_)));
    c.label->set_caption(text);
  }
}
