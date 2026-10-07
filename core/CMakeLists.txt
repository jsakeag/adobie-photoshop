add_library(ps_core STATIC
  src/document.cpp
  src/image.cpp
  src/image_io.cpp
  src/tiled_image.cpp
  src/viewport.cpp
)
target_include_directories(ps_core PUBLIC include)
target_include_directories(ps_core SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_compile_options(ps_core PRIVATE -Wall -Wextra -Wpedantic)
