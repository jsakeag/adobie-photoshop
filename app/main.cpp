#include <cstdio>
#include <cstring>
#include <exception>

#include <nanogui/nanogui.h>

#include "app.h"
#include "selftest.h"

int main(int argc, char** argv) {
  try {
    if (argc == 5 && std::strcmp(argv[1], "--selftest") == 0) {
      return run_selftest(argv[2], argv[3], argv[4]);
    }
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
