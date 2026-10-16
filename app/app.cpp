#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <optional>
#include <utility>

#include <nanogui/label.h>
#include <nanogui/opengl.h>

#include "canvas_view.h"
#include "layers_panel.h"
#include "ps/image_io.h"
#include "ps/layer_commands.h"
#include "ps/selection_commands.h"
#include "tools_panel.h"

namespace {

constexpr int kStatusHeight = 24;

const std::vector<std::pair<std::string, std::string>> kImageTypes = {
    {"png", "PNG"}, {"jpg", "JPEG"}, {"jpeg", "JPEG"},
    {"bmp", "BMP"}, {"tga", "Targa"}, {"gif", "GIF"}};

std::string file_stem(const std::string& path) {
  const std::size_t slash = path.find_last_of("/\\");
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const std::size_t dot = name.find_last_of('.');
  return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

}  // namespace

App::App() : nanogui::Screen(nanogui::Vector2i(1280, 800), "Mock Photoshop") {
  canvas_ = new CanvasView(this);
  canvas_->set_pointer_handler([this](PointerPhase phase, ps::Vec2 p, int mods) {
    switch (phase) {
      case PointerPhase::Press: tool_press(p, mods); break;
      case PointerPhase::Drag: tool_drag(p); break;
      case PointerPhase::Release: tool_release(p); break;
    }
  });
  status_ = new nanogui::Label(this, "");

  ToolsPanelActions tool_actions;
  tool_actions.set_tool = [this](ps::ToolKind k) { set_tool(k); };
  tool_actions.set_color = [this](ps::Color c) { set_color(c); };
  tool_actions.set_brush = [this](ps::BrushSettings b) { set_brush(b); };
  tool_actions.set_tolerance = [this](int t) { set_tolerance(t); };
  tool_actions.set_paint_on_mask = [this](bool m) { set_paint_on_mask(m); };
  tool_actions.select_all = [this] { select_all(); };
  tool_actions.deselect = [this] { deselect(); };
  tool_actions.invert_selection = [this] { invert_selection(); };
  tool_actions.set_feather_radius = [this](float r) { set_feather_radius(r); };
  tool_actions.feather_selection = [this] { feather_selection(); };
  tools_ = new ToolsPanel(this, std::move(tool_actions));
  set_tool(tool_kind_);

  LayersPanelActions actions;
  actions.toggle_undo = [this] { toggle_undo(); };
  actions.select = [this](std::size_t i) { select_layer(i); };
  actions.set_visible = [this](std::size_t i, bool v) { set_layer_visible(i, v); };
  actions.set_opacity = [this](float v) { set_opacity(v); };
  actions.set_blend = [this](ps::BlendMode m) { set_blend(m); };
  actions.new_layer = [this] { new_layer(); };
  actions.delete_layer = [this] { delete_layer(); };
  actions.raise_layer = [this] { raise_layer(); };
  actions.lower_layer = [this] { lower_layer(); };
  actions.open_as_layer = [this] { open_as_layer_dialog(); };
  actions.add_mask = [this](bool reveal) { add_mask(reveal); };
  actions.remove_mask = [this] { remove_mask(); };
  layers_ = new LayersPanel(this, std::move(actions));

  layout_widgets(true);  // before set_document, so the canvas can fit
  set_document(ps::Document(640, 480), "Untitled");
}

bool App::open(const std::string& path) {
  try {
    set_document(ps::Document(ps::load_image(path)), path);
    return true;
  } catch (const std::exception& e) {
    message_ = e.what();
    std::fprintf(stderr, "%s\n", e.what());
    return false;
  }
}

bool App::open_as_layer(const std::string& path) {
  ps::Image image;
  try {
    image = ps::load_image(path);
  } catch (const std::exception& e) {
    message_ = e.what();
    return false;
  }
  const ps::Document& doc = editor_.document();
  ps::Layer layer;
  layer.name = file_stem(path);
  layer.pixels.write(image, {0, 0});
  // Centred on the canvas, as Photoshop places a dragged-in image.
  layer.offset = {(doc.width() - image.width()) / 2,
                  (doc.height() - image.height()) / 2};
  const std::size_t index = doc.layers().empty() ? 0 : selected_ + 1;
  if (!run(std::make_unique<ps::AddLayerCommand>(index, std::move(layer)))) {
    return false;
  }
  selected_ = index;
  return true;
}

void App::toggle_undo() {
  tool_cancel();
  if (editor_.toggle_undo()) {
    message_.clear();
    canvas_stale_ = palettes_stale_ = true;
  }
}

void App::select_layer(std::size_t index) {
  if (index >= editor_.document().layers().size()) return;
  selected_ = index;
  palettes_stale_ = true;
}

void App::set_layer_visible(std::size_t index, bool visible) {
  run(std::make_unique<ps::SetLayerVisibleCommand>(index, visible));
}

void App::set_opacity(float opacity) {
  run(std::make_unique<ps::SetLayerOpacityCommand>(selected_, opacity));
}

void App::set_blend(ps::BlendMode blend) {
  run(std::make_unique<ps::SetLayerBlendCommand>(selected_, blend));
}

void App::new_layer() {
  ps::Layer layer;
  layer.name = "Layer " + std::to_string(editor_.document().layers().size());
  const std::size_t index =
      editor_.document().layers().empty() ? 0 : selected_ + 1;
  if (run(std::make_unique<ps::AddLayerCommand>(index, std::move(layer)))) {
    selected_ = index;
  }
}

void App::delete_layer() {
  if (editor_.document().layers().size() <= 1) return;
  run(std::make_unique<ps::RemoveLayerCommand>(selected_));
}

void App::raise_layer() {
  if (selected_ + 1 >= editor_.document().layers().size()) return;
  if (run(std::make_unique<ps::MoveLayerCommand>(selected_, selected_ + 1))) {
    ++selected_;
  }
}

void App::lower_layer() {
  if (selected_ == 0) return;
  if (run(std::make_unique<ps::MoveLayerCommand>(selected_, selected_ - 1))) {
    --selected_;
  }
}

void App::add_mask(bool reveal_all) {
  run(std::make_unique<ps::SetLayerMaskCommand>(
      selected_, ps::MaskGrid(reveal_all ? 255 : 0)));
}

void App::remove_mask() {
  run(std::make_unique<ps::SetLayerMaskCommand>(selected_, std::nullopt));
}

void App::set_tool(ps::ToolKind kind) {
  tool_cancel();
  tool_kind_ = kind;
  tool_ = ps::make_tool(kind);
  palettes_stale_ = true;
}

void App::set_color(ps::Color color) {
  settings_.color = color;
  palettes_stale_ = true;
}

void App::set_brush(ps::BrushSettings brush) { settings_.brush = brush; }

void App::set_tolerance(int tolerance) { settings_.tolerance = tolerance; }

void App::set_paint_on_mask(bool on_mask) {
  paint_on_mask_ = on_mask;
  palettes_stale_ = true;
}

void App::select_all() { run(std::make_unique<ps::SelectAllCommand>()); }

void App::deselect() { run(std::make_unique<ps::DeselectCommand>()); }

void App::invert_selection() {
  run(std::make_unique<ps::InvertSelectionCommand>());
}

void App::set_feather_radius(float radius) { feather_radius_ = radius; }

void App::feather_selection() {
  run(std::make_unique<ps::FeatherSelectionCommand>(feather_radius_));
}

bool App::paint_on_mask() const {
  const ps::LayerStack& layers = editor_.document().layers();
  return paint_on_mask_ && selected_ < layers.size() &&
         layers.at(selected_).mask.has_value();
}

void App::pick_color(ps::Vec2 point) {
  const ps::Point at{static_cast<int>(std::floor(point.x)),
                     static_cast<int>(std::floor(point.y))};
  if (!editor_.document().bounds().contains(at)) return;
  // The composite as shown, like Photoshop's eyedropper.
  ps::Color c = editor_.document().render({at.x, at.y, 1, 1}).pixel(0, 0);
  c.a = 255;
  set_color(c);
}

void App::tool_press(ps::Vec2 point, int modifiers) {
  if (tool_kind_ == ps::ToolKind::Eyedropper) {
    pick_color(point);
    return;
  }
  if (!tool_ || editor_.document().layers().empty()) return;
  ps::ToolSettings s = settings_;
  s.layer = selected_;
  s.target = paint_on_mask() ? ps::PaintTarget::Mask : ps::PaintTarget::Pixels;
  s.op = (modifiers & GLFW_MOD_SHIFT)   ? ps::SelectionOp::Add
         : (modifiers & GLFW_MOD_ALT) ? ps::SelectionOp::Subtract
                                      : ps::SelectionOp::Replace;
  tool_->press(point, s);
  canvas_stale_ = true;
}

void App::tool_drag(ps::Vec2 point) {
  if (tool_kind_ == ps::ToolKind::Eyedropper) {
    pick_color(point);
    return;
  }
  if (!tool_ || !tool_->active()) return;
  tool_->drag(point);
  canvas_stale_ = true;
}

void App::tool_release(ps::Vec2 point) {
  if (!tool_ || !tool_->active()) return;
  std::unique_ptr<ps::Command> command = tool_->release(point);
  if (command) run(std::move(command));
  canvas_stale_ = true;  // the preview goes away either way
}

void App::tool_cancel() {
  if (tool_ && tool_->active()) {
    tool_->cancel();
    canvas_stale_ = true;
  }
}

bool App::run(std::unique_ptr<ps::Command> command) {
  try {
    editor_.execute(std::move(command));
  } catch (const std::exception& e) {
    message_ = e.what();
    palettes_stale_ = true;
    return false;
  }
  message_.clear();
  canvas_stale_ = palettes_stale_ = true;
  return true;
}

void App::set_document(ps::Document document, const std::string& name) {
  tool_cancel();
  editor_ = ps::Editor(std::move(document));
  selected_ = editor_.document().layers().size() - 1;
  message_.clear();
  canvas_->show(editor_.document().flatten());
  set_caption(name + " - Mock Photoshop");
  canvas_stale_ = palettes_stale_ = true;
}

void App::open_dialog() {
  const std::vector<std::string> paths =
      nanogui::file_dialog(this, nanogui::FileDialogType::Open, kImageTypes);
  if (!paths.empty()) open(paths.front());
}

void App::open_as_layer_dialog() {
  const std::vector<std::string> paths =
      nanogui::file_dialog(this, nanogui::FileDialogType::Open, kImageTypes);
  if (!paths.empty()) open_as_layer(paths.front());
}

void App::refresh() {
  const std::size_t count = editor_.document().layers().size();
  if (count > 0) selected_ = std::min(selected_, count - 1);
  if (canvas_stale_) {
    canvas_stale_ = false;
    show_canvas();
  }
  if (palettes_stale_) {
    palettes_stale_ = false;
    const ps::Document& doc = editor_.document();
    ToolsPanelState state;
    state.tool = tool_kind_;
    state.color = settings_.color;
    state.brush = settings_.brush;
    state.tolerance = settings_.tolerance;
    state.paint_on_mask = paint_on_mask_;
    state.layer_has_mask = selected_ < count && doc.layers().at(selected_).mask;
    state.has_selection = doc.selection().has_value();
    state.feather_radius = feather_radius_;
    tools_->refresh(state);
    layers_->refresh(editor_, selected_);
    perform_layout();
    layout_widgets(false);  // perform_layout resized the canvas; the user may
                            // have dragged the palettes, so leave them there
  }
}

void App::show_canvas() {
  // While a gesture is under way, show what its command would do, run on a
  // copy of the document (cheap: the copy shares every tile).
  std::optional<ps::Document> preview;
  if (tool_ && tool_->active()) {
    if (std::unique_ptr<ps::Command> command = tool_->command()) {
      preview = editor_.document();
      try {
        command->execute(*preview);
      } catch (const std::exception&) {
        preview.reset();
      }
    }
  }
  const ps::Document& shown = preview ? *preview : editor_.document();
  ps::Image image = shown.flatten();
  if (shown.selection()) {
    ps::draw_selection_outline(image, *shown.selection(), {0, 0});
  }
  canvas_->update(image);
}

bool App::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (key == GLFW_KEY_SPACE) {  // hold to pan with the left button
    canvas_->set_space_held(action != GLFW_RELEASE);
    return true;
  }
  if (nanogui::Screen::keyboard_event(key, scancode, action, modifiers)) {
    return true;
  }
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

  if (modifiers & (GLFW_MOD_SUPER | GLFW_MOD_CONTROL)) {
    switch (key) {
      case GLFW_KEY_O: open_dialog(); return true;
      case GLFW_KEY_Z: toggle_undo(); return true;
      case GLFW_KEY_A: select_all(); return true;
      case GLFW_KEY_D: deselect(); return true;
      case GLFW_KEY_I:
        if (modifiers & GLFW_MOD_SHIFT) invert_selection();
        return true;
      case GLFW_KEY_EQUAL: canvas_->zoom_in(); return true;
      case GLFW_KEY_MINUS: canvas_->zoom_out(); return true;
      case GLFW_KEY_0: canvas_->fit(); return true;
      case GLFW_KEY_1: canvas_->actual_size(); return true;
      default: return false;
    }
  }

  // Photoshop's single-letter tool keys; M switches between the marquees.
  switch (key) {
    case GLFW_KEY_M:
      set_tool(tool_kind_ == ps::ToolKind::RectMarquee
                   ? ps::ToolKind::EllipseMarquee
                   : ps::ToolKind::RectMarquee);
      return true;
    case GLFW_KEY_L: set_tool(ps::ToolKind::Lasso); return true;
    case GLFW_KEY_W: set_tool(ps::ToolKind::MagicWand); return true;
    case GLFW_KEY_B: set_tool(ps::ToolKind::Brush); return true;
    case GLFW_KEY_E: set_tool(ps::ToolKind::Eraser); return true;
    case GLFW_KEY_K: set_tool(ps::ToolKind::Bucket); return true;
    case GLFW_KEY_I: set_tool(ps::ToolKind::Eyedropper); return true;
    case GLFW_KEY_ESCAPE: tool_cancel(); return true;
    default: return false;
  }
}

bool App::drop_event(const std::vector<std::string>& filenames) {
  return !filenames.empty() && open(filenames.front());
}

bool App::resize_event(const nanogui::Vector2i& size) {
  nanogui::Screen::resize_event(size);
  layout_widgets(true);
  return true;
}

void App::draw_all() {
  refresh();
  update_status();
  nanogui::Screen::draw_all();
}

void App::capture(const std::string& path) {
  capture_path_ = path;
  redraw();
  draw_all();
  capture_path_.clear();
}

void App::draw_teardown() {
  if (!capture_path_.empty()) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(glfw_window(), &width, &height);
    ps::Image frame(width, height);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame.data());
    // OpenGL rows run bottom to top; make it opaque like the window.
    ps::Image upright = frame.flipped_vertical();
    for (std::size_t i = 3; i < upright.size_bytes(); i += 4) {
      upright.data()[i] = 255;
    }
    ps::save_png(upright, capture_path_);
  }
  nanogui::Screen::draw_teardown();
}

void App::layout_widgets(bool place_palettes) {
  const nanogui::Vector2i size = m_size;
  const int left = ToolsPanel::kWidth;
  const int canvas_width = size.x() - left - LayersPanel::kWidth;
  canvas_->set_position({left, 0});
  canvas_->set_size({canvas_width, size.y() - kStatusHeight});
  if (place_palettes) {
    tools_->set_position({0, 0});
    layers_->set_position({left + canvas_width, 0});
  }
  status_->set_position({8, size.y() - kStatusHeight + 4});
  status_->set_size({size.x() - 16, kStatusHeight - 4});
}

void App::update_status() {
  const ps::Document& document = editor_.document();
  char text[256];
  int n = std::snprintf(text, sizeof(text), "%s    %.4g%%    %d x %d px",
                        ps::tool_name(tool_kind_),
                        canvas_->viewport().zoom() * 100.0, document.width(),
                        document.height());
  const int x = static_cast<int>(std::floor(canvas_->cursor().x));
  const int y = static_cast<int>(std::floor(canvas_->cursor().y));
  if (canvas_->hovering() && x >= 0 && y >= 0 && x < document.width() &&
      y < document.height()) {
    n += std::snprintf(text + n, sizeof(text) - n, "    X: %d  Y: %d", x, y);
  }
  if (!message_.empty() && n < static_cast<int>(sizeof(text))) {
    std::snprintf(text + n, sizeof(text) - n, "    %s", message_.c_str());
  }
  status_->set_caption(text);
}
