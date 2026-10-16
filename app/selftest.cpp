#include "selftest.h"

#include <cstdio>
#include <exception>
#include <string>

#include <nanogui/nanogui.h>
#include <nanogui/opengl.h>

#include "app.h"
#include "ps/blend.h"
#include "ps/tools.h"

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

    // Milestone 5: selection and paint, through canvas gestures.
    const auto pixel = [&](int x, int y) {
      return doc().render({x, y, 1, 1}).pixel(0, 0);
    };
    const auto stroke = [&](ps::Vec2 from, ps::Vec2 to) {
      app->tool_press(from, 0);
      for (int i = 1; i <= 10; ++i) {
        app->tool_drag({from.x + (to.x - from.x) * i / 10,
                        from.y + (to.y - from.y) * i / 10});
      }
      app->tool_release(to);
    };
    const ps::Color red{255, 0, 0, 255};
    const ps::Color yellow{250, 210, 60, 255};

    check(app->open(base_image), "reopen the base image");
    const ps::Color sky = pixel(75, 150);
    app->set_tool(ps::ToolKind::RectMarquee);
    stroke({100, 100}, {250, 200});
    check(doc().selection() && doc().selection()->value(100, 100) == 255 &&
              doc().selection()->value(249, 199) == 255 &&
              doc().selection()->value(250, 150) == 0,
          "marquee drag selects the rectangle");
    shot("7_marquee");

    app->set_tool(ps::ToolKind::Brush);
    app->set_color(red);
    ps::BrushSettings brush;
    brush.diameter = 30;
    app->set_brush(brush);
    stroke({50, 150}, {350, 150});
    check(pixel(175, 150) == red, "brush paints inside the selection");
    check(pixel(75, 150) == sky, "but not outside it");
    shot("8_brush_in_selection");

    app->deselect();
    check(!doc().selection(), "deselect");
    app->tool_press({100, 300}, 0);
    app->tool_drag({300, 250});
    app->tool_drag({500, 300});
    shot("9_stroke_preview");
    check(app->editor().undo_name() == "Deselect",
          "a stroke in progress is only a preview");
    app->tool_cancel();

    app->set_tool(ps::ToolKind::Eyedropper);
    app->tool_press({470, 150}, 0);
    check(app->color() == yellow, "eyedropper picks the sun's colour");

    app->set_tool(ps::ToolKind::MagicWand);
    app->set_tolerance(0);
    stroke({300, 380}, {300, 380});
    check(doc().selection() && doc().selection()->value(10, 400) == 255 &&
              doc().selection()->value(630, 330) == 255 &&
              doc().selection()->value(300, 329) == 0,
          "magic wand selects the ground");
    app->set_tool(ps::ToolKind::Bucket);
    app->set_tolerance(32);
    stroke({300, 380}, {300, 380});
    check(pixel(10, 400) == yellow && pixel(300, 100) != yellow,
          "bucket fills the selected ground");
    shot("10_wand_and_bucket");

    app->deselect();
    check(app->open_as_layer(layer_image), "add the layer again");
    app->add_mask(true);
    app->set_paint_on_mask(true);
    app->set_tool(ps::ToolKind::Brush);
    app->set_color(ps::kBlack);
    brush.diameter = 40;
    app->set_brush(brush);
    const ps::Color behind = pixel(320, 210);
    stroke({180, 210}, {460, 210});
    check(doc().layers().at(1).mask->value(150, 100) == 0,
          "black paints the mask to hidden");
    check(pixel(320, 210) != behind, "the hidden part shows the background");

    app->set_paint_on_mask(false);
    app->set_tool(ps::ToolKind::Eraser);
    const ps::Color before_erase = pixel(320, 130);
    stroke({180, 130}, {460, 130});
    check(doc().layers().at(1).pixels.pixel(150, 20).a == 0,
          "eraser clears the layer's pixels");
    check(pixel(320, 130) != before_erase, "and the background shows");
    shot("11_mask_and_eraser");
    app->toggle_undo();
    check(pixel(320, 130) == before_erase, "Cmd+Z brings the erased part back");
    app->toggle_undo();
    check(doc().layers().at(1).pixels.pixel(150, 20).a == 0,
          "and redo erases again");
  }
  nanogui::shutdown();
  std::printf("%s\n", failures == 0 ? "selftest passed" : "selftest FAILED");
  return failures == 0 ? 0 : 1;
}
