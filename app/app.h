#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <nanogui/screen.h>

#include "ps/editor.h"

class CanvasView;
class LayersPanel;
namespace nanogui {
class Label;
}

// The main window: canvas, Layers palette and status bar.
//
// Every edit is a command run through editor_. The public actions are what
// the widgets and shortcuts call, so the self-test can drive the same paths.
class App : public nanogui::Screen {
 public:
  App();

  const ps::Editor& editor() const { return editor_; }
  // The layer the palette controls act on (index, bottom to top).
  std::size_t selected() const { return selected_; }
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
  // Brings the canvas and palette up to date with the editor. Widgets are
  // rebuilt here, outside their own callbacks.
  void refresh();
  // Sizes the canvas and status bar; also docks the palette if asked.
  void layout_widgets(bool place_panel);
  void update_status();

  ps::Editor editor_{ps::Document(1, 1)};
  std::size_t selected_ = 0;
  std::string message_;
  bool stale_ = true;  // canvas and palette need refresh()
  std::string capture_path_;
  CanvasView* canvas_ = nullptr;
  LayersPanel* panel_ = nullptr;
  nanogui::Label* status_ = nullptr;
};
