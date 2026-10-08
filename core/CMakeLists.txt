add_library(ps_core STATIC
  src/blend.cpp
  src/compositor.cpp
  src/document.cpp
  src/image.cpp
  src/image_io.cpp
  src/layer_stack.cpp
  src/mask_grid.cpp
  src/tile_grid.cpp
  src/viewport.cpp
)
target_include_directories(ps_core PUBLIC include)
target_include_directories(ps_core SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
# No fused multiply-add: blend results must match the goldens on every CPU.
target_compile_options(ps_core PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
