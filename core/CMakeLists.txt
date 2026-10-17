add_library(ps_core STATIC
  src/adjust_commands.cpp
  src/adjustments.cpp
  src/blend.cpp
  src/brush.cpp
  src/command.cpp
  src/compositor.cpp
  src/document.cpp
  src/editor.cpp
  src/filter_commands.cpp
  src/filters.cpp
  src/image.cpp
  src/image_io.cpp
  src/layer_stack.cpp
  src/mask_grid.cpp
  src/paint.cpp
  src/selection.cpp
  src/tile_grid.cpp
  src/tools.cpp
  src/transform.cpp
  src/viewport.cpp
)
target_include_directories(ps_core PUBLIC include)
target_include_directories(ps_core SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
# No fused multiply-add: blend results must match the goldens on every CPU.
target_compile_options(ps_core PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
