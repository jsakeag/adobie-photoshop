#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <nanogui/window.h>

#include "effect.h"

namespace nanogui {
class ComboBox;
class Label;
class Slider;
}  // namespace nanogui

// What the Adjustments & Filters palette does. Changing a control previews
// the effect; Apply turns it into a command; Cancel drops it.
struct EffectsPanelActions {
  std::function<void(const Effect&)> preview;
  std::function<void()> apply;
  std::function<void()> cancel;
};

// The Adjustments & Filters palette: pick an effect, set it with sliders
// while the canvas previews it, then Apply (one undo step) or Cancel.
class EffectsPanel : public nanogui::Window {
 public:
  static constexpr int kWidth = 240;

  EffectsPanel(nanogui::Widget* parent, EffectsPanelActions actions);

  // Shows `effect`. Rebuilds the sliders only when its kind changed, so it
  // is safe to call while a slider is being dragged.
  void show(const Effect& effect);

  // Opens or closes the Adjust menu (or the Filter menu), for the
  // self-test's screenshots.
  void open_menu(bool filters, bool open);

 private:
  struct Control {
    std::string caption;
    float lo, hi;
    int decimals;  // shown after the point
    std::function<float(const Effect&)> get;
    std::function<void(Effect&, float)> set;
    nanogui::Label* label = nullptr;
    nanogui::Slider* slider = nullptr;
  };

  void rebuild();
  void update_labels();

  EffectsPanelActions actions_;
  Effect effect_;
  std::size_t kind_ = static_cast<std::size_t>(-1);
  // Two menus, as in Photoshop's Image > Adjust and Filter: each starts
  // with a placeholder item that shows while the other menu's kind is
  // chosen.
  nanogui::ComboBox* adjust_menu_ = nullptr;
  nanogui::ComboBox* filter_menu_ = nullptr;
  nanogui::Widget* controls_ = nullptr;
  std::vector<Control> sliders_;
};
