#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nanogui/screen.h>

#include "effect.h"
#include "ps/editor.h"
#include "ps/tools.h"

class EffectsPanel;
class CanvasView;
class LayersPanel;
class ToolsPanel;
namespace nanogui {
class Label;
}

// The main window: Tools palette, canvas, Layers palette and status bar.
//
// Every edit is a command run through editor_. The public actions are what
// the widgets, shortcuts and canvas gestures call, so the self-test can
// drive the same paths.
class App : public nanogui::Screen {
 public:
  App();

  const ps::Editor& editor() const { return editor_; }
  // The layer the palettes and tools act on (index, bottom to top).
  std::size_t selected() const { return selected_; }
  ps::ToolKind tool() const { return tool_kind_; }
  ps::Color color() const { return settings_.color; }
  // The last error or note shown in the status bar; empty if none.
  const std::string& message() const { return message_; }

  // File. Return false and show the error in the status bar on failure.
  bool open(const std::string& path);
  bool open_as_layer(const std::string& path);

  // Edit.
  void toggle_undo();

  // Layers palette.
  void select_layer(std::size_t index);
  void set_layer_visible(std::size_t index, bool visible);
  void set_opacity(float opacity);
  void set_blend(ps::BlendMode blend);
  void new_layer();
  void delete_layer();
  void raise_layer();
  void lower_layer();
  void add_mask(bool reveal_all);
  void remove_mask();

  // Tools palette.
  void set_tool(ps::ToolKind kind);
  void set_color(ps::Color color);
  void set_brush(ps::BrushSettings brush);
  void set_tolerance(int tolerance);
  void set_paint_on_mask(bool on_mask);
  void select_all();
  void deselect();
  void invert_selection();
  void set_feather_radius(float radius);
  void feather_selection();

  // Adjustments & Filters palette. A previewed effect shows on the canvas
  // until it is applied (one command on the selected layer) or cancelled.
  void preview_effect(const Effect& effect);
  void apply_effect();
  void cancel_effect();
  bool previewing_effect() const { return pending_.has_value(); }
  // Opens or closes the palette's Adjust or Filter menu (for the self-test).
  void open_effect_menu(bool filters, bool open);

  // Canvas gestures, in document coordinates. Shift adds to a selection,
  // Alt (Option) subtracts from it.
  void tool_press(ps::Vec2 point, int modifiers);
  void tool_drag(ps::Vec2 point);
  void tool_release(ps::Vec2 point);
  void tool_cancel();

  // Draws one frame and saves exactly what is on screen as a PNG.
  void capture(const std::string& path);

  bool keyboard_event(int key, int scancode, int action, int modifiers) override;
  bool drop_event(const std::vector<std::string>& filenames) override;
  bool resize_event(const nanogui::Vector2i& size) override;
  void draw_all() override;
  void draw_teardown() override;

 private:
  // Runs a command; on failure shows the error and leaves things as they
  // were. Returns whether it succeeded.
  bool run(std::unique_ptr<ps::Command> command);
  void set_document(ps::Document document, const std::string& name);
  void open_dialog();
  void open_as_layer_dialog();
  bool paint_on_mask() const;
  void pick_color(ps::Vec2 point);
  // Brings the canvas and palettes up to date. Widgets are rebuilt here,
  // outside their own callbacks.
  void refresh();
  void show_canvas();
  // Sizes the canvas and status bar; also docks the palettes if asked.
  void layout_widgets(bool place_palettes);
  void update_status();

  ps::Editor editor_{ps::Document(1, 1)};
  std::size_t selected_ = 0;
  std::string message_;

  ps::ToolKind tool_kind_ = ps::ToolKind::Brush;
  std::unique_ptr<ps::Tool> tool_;
  ps::ToolSettings settings_;  // colour, brush and tolerance; the rest is
                               // filled in when a gesture starts
  bool paint_on_mask_ = false;
  Effect effect_ = ps::Levels{};    // what the palette shows
  std::optional<Effect> pending_;   // previewed, not yet applied
  std::uint32_t noise_seed_ = 1;    // so each Add Noise differs
  float feather_radius_ = 5.0f;

  bool canvas_stale_ = true;    // the picture needs redrawing
  bool palettes_stale_ = true;  // the palettes need rebuilding
  std::string capture_path_;
  CanvasView* canvas_ = nullptr;
  ToolsPanel* tools_ = nullptr;
  LayersPanel* layers_ = nullptr;
  EffectsPanel* effects_ = nullptr;
  nanogui::Label* status_ = nullptr;
};
