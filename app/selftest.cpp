#include "selftest.h"

#include <cstdio>
#include <exception>
#include <string>

#include <nanogui/nanogui.h>
#include <nanogui/opengl.h>

#include "app.h"
#include "ps/blend.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

}  // namespace

int run_selftest(const std::string& out_dir, const std::string& base_image,
                 const std::string& layer_image) {
  nanogui::init();
  {
    nanogui::ref<App> app = new App();
    app->set_visible(true);
    for (int i = 0; i < 5; ++i) glfwPollEvents();
    const auto& doc = [&]() -> const ps::Document& {
      return app->editor().document();
    };
    const auto shot = [&](const char* name) {
      app->capture(out_dir + "/" + name + ".png");
    };

    check(app->open(base_image), "open the base image");
    check(doc().layers().size() == 1, "one layer after open");
    shot("1_open");

    check(app->open_as_layer(layer_image), "open an image as a layer");
    check(doc().layers().size() == 2 && app->selected() == 1,
          "the new layer is on top and selected");
    app->set_blend(ps::BlendMode::Multiply);
    check(doc().layers().at(1).blend == ps::BlendMode::Multiply,
          "blend mode set to Multiply");
    app->set_opacity(0.6f);
    check(doc().layers().at(1).opacity == 0.6f, "opacity set to 60%");
    shot("2_multiply_60");

    app->add_mask(false);
    check(doc().layers().at(1).mask && doc().layers().at(1).mask->background() == 0,
          "hide-all mask added");
    shot("3_hide_all_mask");

    app->toggle_undo();
    check(!doc().layers().at(1).mask && app->editor().can_redo(),
          "Cmd+Z undoes the mask");
    shot("4_mask_undone");
    app->toggle_undo();
    check(doc().layers().at(1).mask.has_value(), "Cmd+Z again redoes it");
    app->remove_mask();
    check(!doc().layers().at(1).mask, "mask removed");

    app->new_layer();
    check(doc().layers().size() == 3 && app->selected() == 2,
          "new empty layer on top, selected");
    app->lower_layer();
    check(app->selected() == 1 && doc().layers().at(1).name == "Layer 2",
          "lowered one step, selection follows");
    app->delete_layer();
    check(doc().layers().size() == 2, "deleted");
    app->set_layer_visible(0, false);
    check(!doc().layers().at(0).visible, "background hidden");
    shot("5_background_hidden");
    app->set_layer_visible(0, true);

    app->select_layer(0);
    app->delete_layer();
    app->delete_layer();
    check(doc().layers().size() == 1, "the last layer cannot be deleted");

    check(!app->open_as_layer("/nonexistent.png") && !app->message().empty(),
          "a bad file shows an error instead of crashing");
    shot("6_error_message");
  }
  nanogui::shutdown();
  std::printf("%s\n", failures == 0 ? "selftest passed" : "selftest FAILED");
  return failures == 0 ? 0 : 1;
}
