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
  src/native_format.cpp
  src/paint.cpp
  src/selection.cpp
  src/tile_grid.cpp
  src/tools.cpp
  src/transform.cpp
  src/viewport.cpp
  ${PROJECT_SOURCE_DIR}/third_party/miniz/miniz.c
)
target_include_directories(ps_core PUBLIC include)
target_include_directories(ps_core SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
# No fused multiply-add: blend results must match the goldens on every CPU.
target_compile_options(ps_core PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)

# The native format: zip (vendored miniz), JSON (vendored nlohmann) and zstd
# from the system (brew install zstd).
target_include_directories(ps_core SYSTEM PRIVATE
  ${PROJECT_SOURCE_DIR}/third_party/miniz
  ${PROJECT_SOURCE_DIR}/third_party)
set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/miniz/miniz.c
  PROPERTIES COMPILE_OPTIONS "-w")
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
target_link_libraries(ps_core PUBLIC PkgConfig::ZSTD)
