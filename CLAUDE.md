# CLAUDE.md: Photoshop 3.0-4.0 Clone

## Project summary
A raster image editor modeled on Photoshop 3.0-4.0 (layers, masks, blend modes, selections, adjustments, filters, transform). Budget: ~25 hours over ~6 weeks, so work in small, test-backed slices and keep this file current.

**Current milestone:** 1 (Canvas): code complete, viewer not yet checked by eye
**Next step:** Run `./build/app/mock_photoshop <image>` and confirm it draws, pans (drag), zooms (scroll, Cmd +/-/0/1) and shows cursor coordinates. Fix anything off, then write the milestone list into this file and start milestone 2.

## Build and test
```
cmake -S . -B build && cmake --build build -j
./build/tests/ps_tests                           # full suite
PS_UPDATE_GOLDEN=1 ./build/tests/ps_tests "[golden]"   # rewrite goldens, then look at them
cmake -S . -B build -DPS_BUILD_APP=OFF           # core and tests only, skips NanoGUI
```

## Session protocol
1. Start: read this file, run the full test suite, confirm it's green.
2. Work on the current milestone only. Do not start the next one early.
3. End: run tests, commit, update "Current milestone" and "Next step" above.
4. If a milestone runs over its estimate by 50%, cut scope within it. Do not borrow time from the next one.
5. Keep ~2 hours of buffer. Do not spend it on features.

## Stack
- **Core:** C++17 or later, headless, no UI dependencies
- **UI:** NanoGUI (OpenGL), kept thin
- **Bindings:** nanobind (minimal test harness first)
- **Image I/O:** stb_image / libpng / libjpeg-turbo
- **Native format compression:** zstd or lz4
- **Serialization:** nlohmann/json
- **Math:** GLM or Eigen
- **Threading:** std::thread plus a simple pool
- **Tests:** Catch2 or GoogleTest, plus golden images

## Architecture
- `core/` (`ps_core`, namespace `ps`): headless, no UI or GL includes. Everything testable lives here.
  - `Image`: owning RGBA8 buffer, straight alpha, top-left origin.
  - `Document`: the open image plus canvas operations (resize with anchor, crop, rotate, flip). Holds one `Image` until layers arrive.
  - `Viewport`: zoom/pan math, `screen = doc * zoom + offset`, zoom limited to 1:16 to 16:1.
  - `image_io`: load and save through stb (compiled static, NanoGUI bundles its own copy).
- `app/` (`mock_photoshop`): NanoGUI shell. `CanvasView` draws through `nanogui::ImageView` but takes zoom and pan from `ps::Viewport`. No editing logic here.
- `tests/`: Catch2 v3. Golden PNGs in `tests/golden/`, compared exactly; mismatches are written to `build/tests/output/`.
- `third_party/stb/`: vendored headers. Catch2 and NanoGUI come through FetchContent; NanoGUI is pinned to a commit because later ones don't compile with the OpenGL backend on macOS.