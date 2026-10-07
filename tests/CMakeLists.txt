include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.1
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS 3
)
FetchContent_MakeAvailable(Catch2)

add_executable(ps_tests
  golden.cpp
  test_blend.cpp
  test_compositor.cpp
  test_document.cpp
  test_document_layers.cpp
  test_image.cpp
  test_image_io.cpp
  test_layer_stack.cpp
  test_tile_grid.cpp
  test_viewport.cpp
)
target_link_libraries(ps_tests PRIVATE ps_core Catch2::Catch2WithMain)
target_compile_definitions(ps_tests PRIVATE
  PS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
  PS_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)

add_test(NAME ps_tests COMMAND ps_tests)
