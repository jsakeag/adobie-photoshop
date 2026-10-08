#include <cmath>
#include <cstdio>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include <nanogui/label.h>
#include <nanogui/nanogui.h>
#include <nanogui/opengl.h>

#include "canvas_view.h"
#include "ps/document.h"
#include "ps/editor.h"
#include "ps/image_io.h"

namespace {

constexpr int kStatusHeight = 24;

class App : public nanogui::Screen {
 public:
  App() : nanogui::Screen(nanogui::Vector2i(1024, 720), "Mock Photoshop") {
    canvas_ = new CanvasView(this);
    status_ = new nanogui::Label(this, "");
    layout_widgets();
    set_document(ps::Document(640, 480), "Untitled");
  }

  bool open(const std::string& path) {
    try {
      set_document(ps::Document(ps::load_image(path)), path);
      return true;
    } catch (const std::exception& e) {
      std::fprintf(stderr, "%s\n", e.what());
      return false;
    }
  }

  bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (nanogui::Screen::keyboard_event(key, scancode, action, modifiers)) {
      return true;
    }
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;
    if (!(modifiers & (GLFW_MOD_SUPER | GLFW_MOD_CONTROL))) return false;

    switch (key) {
      case GLFW_KEY_O: open_dialog(); return true;
      case GLFW_KEY_EQUAL: canvas_->zoom_in(); return true;
      case GLFW_KEY_MINUS: canvas_->zoom_out(); return true;
      case GLFW_KEY_0: canvas_->fit(); return true;
      case GLFW_KEY_1: canvas_->actual_size(); return true;
      default: return false;
    }
  }

  bool drop_event(const std::vector<std::string>& filenames) override {
    return !filenames.empty() && open(filenames.front());
  }

  bool resize_event(const nanogui::Vector2i& size) override {
    nanogui::Screen::resize_event(size);
    layout_widgets();
    return true;
  }

  void draw_all() override {
    update_status();
    nanogui::Screen::draw_all();
  }

 private:
  void set_document(ps::Document document, const std::string& name) {
    editor_ = ps::Editor(std::move(document));
    canvas_->show(editor_.document().flatten());
    set_caption(name + " - Mock Photoshop");
  }

  void open_dialog() {
    const std::vector<std::string> paths = nanogui::file_dialog(
        this, nanogui::FileDialogType::Open,
        {{"png", "PNG"}, {"jpg", "JPEG"}, {"jpeg", "JPEG"}, {"bmp", "BMP"},
         {"tga", "Targa"}, {"gif", "GIF"}});
    if (!paths.empty()) open(paths.front());
  }

  void layout_widgets() {
    const nanogui::Vector2i size = m_size;
    canvas_->set_position({0, 0});
    canvas_->set_size({size.x(), size.y() - kStatusHeight});
    status_->set_position({8, size.y() - kStatusHeight + 4});
    status_->set_size({size.x() - 16, kStatusHeight - 4});
  }

  void update_status() {
    const ps::Document& document = editor_.document();
    char text[128];
    int n = std::snprintf(text, sizeof(text), "%.4g%%    %d x %d px",
                          canvas_->viewport().zoom() * 100.0, document.width(),
                          document.height());
    const int x = static_cast<int>(std::floor(canvas_->cursor().x));
    const int y = static_cast<int>(std::floor(canvas_->cursor().y));
    if (canvas_->hovering() && x >= 0 && y >= 0 && x < document.width() &&
        y < document.height()) {
      std::snprintf(text + n, sizeof(text) - n, "    X: %d  Y: %d", x, y);
    }
    status_->set_caption(text);
  }

  // The UI reads editor_.document() and changes it only through commands.
  ps::Editor editor_{ps::Document(1, 1)};
  CanvasView* canvas_ = nullptr;
  nanogui::Label* status_ = nullptr;
};

}  // namespace

int main(int argc, char** argv) {
  try {
    nanogui::init();
    {
      nanogui::ref<App> app = new App();
      if (argc > 1 && !app->open(argv[1])) {
        nanogui::shutdown();
        return 1;
      }
      app->set_visible(true);
      nanogui::run();
    }
    nanogui::shutdown();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Fatal error: %s\n", e.what());
    return 1;
  }
  return 0;
}
