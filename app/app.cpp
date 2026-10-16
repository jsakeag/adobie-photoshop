#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <utility>

#include <nanogui/label.h>
#include <nanogui/opengl.h>

#include "canvas_view.h"
#include "layers_panel.h"
#include "ps/image_io.h"
#include "ps/layer_commands.h"

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

App::App() : nanogui::Screen(nanogui::Vector2i(1100, 760), "Mock Photoshop") {
  canvas_ = new CanvasView(this);
  status_ = new nanogui::Label(this, "");

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
  panel_ = new LayersPanel(this, std::move(actions));

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
  if (editor_.toggle_undo()) {
    message_.clear();
    stale_ = true;
  }
}

void App::select_layer(std::size_t index) {
  if (index >= editor_.document().layers().size()) return;
  selected_ = index;
  stale_ = true;
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

bool App::run(std::unique_ptr<ps::Command> command) {
  try {
    editor_.execute(std::move(command));
  } catch (const std::exception& e) {
    message_ = e.what();
    return false;
  }
  message_.clear();
  stale_ = true;
  return true;
}

void App::set_document(ps::Document document, const std::string& name) {
  editor_ = ps::Editor(std::move(document));
  selected_ = editor_.document().layers().size() - 1;
  message_.clear();
  canvas_->show(editor_.document().flatten());
  set_caption(name + " - Mock Photoshop");
  stale_ = true;
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
  if (!stale_) return;
  stale_ = false;
  const std::size_t count = editor_.document().layers().size();
  if (count > 0) selected_ = std::min(selected_, count - 1);
  canvas_->update(editor_.document().flatten());
  panel_->refresh(editor_, selected_);
  perform_layout();
  layout_widgets(false);  // perform_layout resized the canvas; the user may
                          // have dragged the palette, so leave it there
}

bool App::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (nanogui::Screen::keyboard_event(key, scancode, action, modifiers)) {
    return true;
  }
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;
  if (!(modifiers & (GLFW_MOD_SUPER | GLFW_MOD_CONTROL))) return false;

  switch (key) {
    case GLFW_KEY_O: open_dialog(); return true;
    case GLFW_KEY_Z: toggle_undo(); return true;
    case GLFW_KEY_EQUAL: canvas_->zoom_in(); return true;
    case GLFW_KEY_MINUS: canvas_->zoom_out(); return true;
    case GLFW_KEY_0: canvas_->fit(); return true;
    case GLFW_KEY_1: canvas_->actual_size(); return true;
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

void App::layout_widgets(bool place_panel) {
  const nanogui::Vector2i size = m_size;
  const int canvas_width = size.x() - LayersPanel::kWidth;
  canvas_->set_position({0, 0});
  canvas_->set_size({canvas_width, size.y() - kStatusHeight});
  if (place_panel) panel_->set_position({canvas_width, 0});
  status_->set_position({8, size.y() - kStatusHeight + 4});
  status_->set_size({size.x() - 16, kStatusHeight - 4});
}

void App::update_status() {
  const ps::Document& document = editor_.document();
  char text[256];
  int n = std::snprintf(text, sizeof(text), "%.4g%%    %d x %d px",
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
