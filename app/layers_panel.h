#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include <nanogui/window.h>

#include "ps/blend.h"
#include "ps/editor.h"

namespace nanogui {
class Button;
class ComboBox;
class Label;
class Slider;
}  // namespace nanogui

// What the panel's controls do. The app implements each as a command.
struct LayersPanelActions {
  std::function<void()> toggle_undo;
  std::function<void(std::size_t)> select;
  std::function<void(std::size_t, bool)> set_visible;
  std::function<void(float)> set_opacity;  // selected layer, 0..1
  std::function<void(ps::BlendMode)> set_blend;
  std::function<void()> new_layer;
  std::function<void()> delete_layer;
  std::function<void()> raise_layer;
  std::function<void()> lower_layer;
  std::function<void(std::size_t from, std::size_t to)> move_layer;
  std::function<void()> open_as_layer;
  std::function<void(bool reveal)> add_mask;
  std::function<void()> remove_mask;
};

// The Layers palette: undo, the layer list (top layer first) and controls
// for the selected layer. It only displays state and calls actions.
class LayersPanel : public nanogui::Window {
 public:
  static constexpr int kWidth = 240;

  LayersPanel(nanogui::Widget* parent, LayersPanelActions actions);

  // Rebuilds the list and controls from the editor. Do not call from inside
  // one of the panel's own callbacks: it deletes and recreates widgets.
  void refresh(const ps::Editor& editor, std::size_t selected);

  // Where a layer's row is on screen (window coordinates), for the
  // self-test to drag it.
  nanogui::Vector2i row_center(std::size_t index) const;
  // Opens or closes the blend mode menu, for the self-test's screenshots.
  void open_blend_menu(bool open);

 private:
  LayersPanelActions actions_;
  nanogui::Button* undo_ = nullptr;
  nanogui::ComboBox* blend_ = nullptr;
  nanogui::Label* opacity_label_ = nullptr;
  nanogui::Slider* opacity_ = nullptr;
  nanogui::Widget* rows_ = nullptr;
  nanogui::Button* delete_ = nullptr;
  nanogui::Button* raise_ = nullptr;
  nanogui::Button* lower_ = nullptr;
  nanogui::Button* reveal_mask_ = nullptr;
  nanogui::Button* hide_mask_ = nullptr;
  nanogui::Button* remove_mask_ = nullptr;
  std::vector<int> thumbnails_;  // NanoVG images for the rows
};
