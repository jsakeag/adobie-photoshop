#pragma once

#include <functional>
#include <vector>

#include <nanogui/window.h>

#include "ps/brush.h"
#include "ps/canvas_commands.h"
#include "ps/tools.h"

namespace nanogui {
class Button;
class ColorPicker;
class Label;
class Slider;
}  // namespace nanogui

// What the Tools palette's controls do. Tool choice and settings are UI
// state; the selection buttons run commands.
struct ToolsPanelActions {
  std::function<void()> open;
  std::function<void()> save;
  std::function<void()> save_as;
  std::function<void()> export_image;
  std::function<void(ps::ToolKind)> set_tool;
  std::function<void(ps::Color)> set_color;
  std::function<void(ps::BrushSettings)> set_brush;
  std::function<void(int)> set_tolerance;
  std::function<void(bool)> set_paint_on_mask;
  std::function<void()> select_all;
  std::function<void()> deselect;
  std::function<void()> invert_selection;
  std::function<void(float)> set_feather_radius;
  std::function<void()> feather_selection;
  std::function<void(ps::Rotation)> rotate_canvas;
  std::function<void(ps::FlipAxis)> flip_canvas;
  std::function<void()> crop_to_selection;
};

// What the palette shows.
struct ToolsPanelState {
  ps::ToolKind tool = ps::ToolKind::Brush;
  ps::Color color = ps::kBlack;
  ps::BrushSettings brush;
  int tolerance = 32;
  bool paint_on_mask = false;
  bool layer_has_mask = false;
  bool has_selection = false;
  float feather_radius = 5.0f;
};

// The Tools palette: toolbox, foreground colour, tool options and the
// Select menu's commands.
class ToolsPanel : public nanogui::Window {
 public:
  static constexpr int kWidth = 200;

  ToolsPanel(nanogui::Widget* parent, ToolsPanelActions actions);

  // Shows `state`. Safe to call from anywhere: it only updates widgets.
  void refresh(const ToolsPanelState& state);

 private:
  nanogui::Slider* slider(const char* caption, float lo, float hi,
                          std::function<void(float)> changed,
                          nanogui::Label** label);

  ToolsPanelActions actions_;
  ToolsPanelState state_;
  std::vector<nanogui::Button*> tool_buttons_;
  nanogui::ColorPicker* color_ = nullptr;
  nanogui::Label* size_label_ = nullptr;
  nanogui::Slider* size_ = nullptr;
  nanogui::Label* hardness_label_ = nullptr;
  nanogui::Slider* hardness_ = nullptr;
  nanogui::Label* opacity_label_ = nullptr;
  nanogui::Slider* opacity_ = nullptr;
  nanogui::Label* tolerance_label_ = nullptr;
  nanogui::Slider* tolerance_ = nullptr;
  nanogui::Button* on_layer_ = nullptr;
  nanogui::Button* on_mask_ = nullptr;
  nanogui::Button* inverse_ = nullptr;
  nanogui::Label* feather_label_ = nullptr;
  nanogui::Slider* feather_radius_ = nullptr;
  nanogui::Button* feather_ = nullptr;
  nanogui::Button* crop_ = nullptr;
};
