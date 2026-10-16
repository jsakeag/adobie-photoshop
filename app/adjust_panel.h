#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <nanogui/window.h>

#include "ps/adjustments.h"

namespace nanogui {
class ComboBox;
class Label;
class Slider;
}  // namespace nanogui

// What the Adjustments palette does. Changing a control previews the
// adjustment; Apply turns it into a command; Cancel drops it.
struct AdjustPanelActions {
  std::function<void(const ps::Adjustment&)> preview;
  std::function<void()> apply;
  std::function<void()> cancel;
};

// The Adjustments palette: pick an adjustment, set it with sliders while the
// canvas previews it, then Apply (one undo step) or Cancel.
class AdjustPanel : public nanogui::Window {
 public:
  static constexpr int kWidth = 240;

  AdjustPanel(nanogui::Widget* parent, AdjustPanelActions actions);

  // Shows `adjustment`. Rebuilds the sliders only when its kind changed, so
  // it is safe to call while a slider is being dragged.
  void show(const ps::Adjustment& adjustment);

 private:
  struct Control {
    std::string caption;
    float lo, hi;
    std::function<float(const ps::Adjustment&)> get;
    std::function<void(ps::Adjustment&, float)> set;
    nanogui::Label* label = nullptr;
    nanogui::Slider* slider = nullptr;
  };

  void rebuild();
  void update_labels();

  AdjustPanelActions actions_;
  ps::Adjustment adjustment_;
  std::size_t kind_ = static_cast<std::size_t>(-1);
  nanogui::ComboBox* kinds_ = nullptr;
  nanogui::Widget* controls_ = nullptr;
  std::vector<Control> sliders_;
};
