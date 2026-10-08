# CLAUDE.md: Photoshop 3.0-4.0 Clone

## Project summary
A raster image editor modeled on Photoshop 3.0-4.0 (layers, masks, blend modes, selections, adjustments, filters, transform). Budget: ~25 hours over ~6 weeks, so work in small, test-backed slices and keep this file current.

**Current milestone:** 3 (Blend modes + masks): not started
**Next step:** Add the ten blend modes to `blend.cpp`, one golden each, then per-layer grayscale masks (needs a single-channel `TileGrid`). Open question carried over from milestone 2: the compositor is single-threaded; decide which milestone the thread pool belongs to.

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
3. End: run tests, update README.md (status line, and usage for anything user-facing that changed; compile any code sample you touch), commit, update "Current milestone" and "Next step" above.
4. If a milestone runs over its estimate by 50%, cut scope within it. Do not borrow time from the next one.
5. Keep ~2 hours of buffer. Do not spend it on features.

## Stack
- **Core:** C++17 or later, headless, no UI dependencies
- **UI:** NanoGUI (OpenGL), kept thin; pinned to a commit that compiles with the OpenGL backend on macOS
- **Bindings:** nanobind (minimal test harness first)
- **Image I/O:** stb_image / stb_image_write (vendored in `third_party/stb/`), libpng / libjpeg-turbo if needed later
- **Native format compression:** zstd or lz4
- **Serialization:** nlohmann/json
- **Math:** GLM or Eigen (undecided)
- **Threading:** std::thread plus a simple pool
- **Tests:** Catch2 v3 plus golden images

## Architecture (as built)
- `core/` (`ps_core`, namespace `ps`): headless, no UI or GL includes. Everything testable lives here.
  - `Image`: owning flat RGBA8 buffer, straight alpha, top-left origin. Not document storage: it carries pixels in and out of a `TileGrid` (file I/O, texture upload, regions).
  - `TileGrid`: document pixel storage. Sparse and unbounded (negative coordinates allowed), 64x64 tiles allocated on first write, copy-on-write through `shared_ptr`. Missing tiles read as transparent. `read(Rect)` and `write(Image, Point)` exchange flat regions. Tile coordinates (`TileCoord`, `tile_index`, `tile_rect`) are public so the compositor can cache per tile. RGBA only so far; the 8-bit single-channel variant for masks and selections is milestone 3 work.
  - `blend`: `BlendMode` (Normal only so far) and the pure pixel functions: `premultiply`, `unpremultiply`, `blend(mode, dst, src)` on premultiplied float RGBA.
  - `Layer`: name, `TileGrid` pixels, offset, opacity (0..1), visible, blend. No mask yet.
  - `LayerStack`: layers bottom to top; insert, remove, move.
  - `Compositor`: flattens a stack per 64x64 document tile, bottom-up, and caches the result. It does not watch layers: callers `invalidate(Rect)` and only those tiles are redone. Single-threaded.
  - `Document`: canvas size, `LayerStack` and its `Compositor`. Layers are read through `layers()` and changed only through `Document` methods (`add_layer`, `remove_layer`, `move_layer`, `set_layer_*`, `edit_layer_pixels(index, area)`), which invalidate the right tiles. `render(Rect)` and `flatten()` return the composite. These methods are what milestone 4's commands will call. Canvas operations (resize with anchor, crop, rotate, flip) apply to every layer; layers keep pixels that fall outside the canvas. *(These canvas ops are milestone 8 work done early; don't add more out-of-scope features.)*
  - `Viewport`: zoom/pan math, `screen = doc * zoom + offset`, zoom limited to 1:16 to 16:1.
  - `image_io`: load (PNG/JPEG/BMP/TGA/GIF) and PNG save through stb (compiled static; NanoGUI bundles its own copy). *(Overlaps milestone 9; it exists because the viewer and golden tests need it. JPEG export and the native format are still milestone 9.)*
- `app/` (`mock_photoshop`): NanoGUI shell. `CanvasView` draws through `nanogui::ImageView` but takes zoom and pan from `ps::Viewport`. It uploads the flattened composite as one texture when a file is opened; nothing in the UI edits a document yet, so there is no per-tile re-upload. No editing logic here.
- `tests/`: Catch2 v3. Golden PNGs in `tests/golden/`, compared exactly; mismatches are written to `build/tests/output/`.
- `third_party/stb/`: vendored headers. Catch2 and NanoGUI come through FetchContent.

## Layering
```
UI (NanoGUI): widgets, palettes, canvas view
Command layer: tools, menus (all edits go through here)
Core (C++): document, layers, tiles, compositor, selection, filters, undo
Bindings (nanobind): Python scripting and tests
```

### Hard rules
1. **Core is headless.** No UI includes in `core/`.
2. **Every edit is a Command** (name, parameters, execute, undo). Tools and menus never mutate document state directly.
3. **UI only reads document state and issues commands.**
4. **All blend modes live in one file** (`blend.cpp`) as pure functions on pixel pairs, each with a golden test.
5. **No feature work without a test** (unit, golden image, or round-trip).
6. **Do not implement features outside the current milestone.**

## Data model (target)
```
Document
 ├─ size, mode (RGB 8-bit only), resolution
 ├─ LayerStack (ordered bottom to top)
 │    └─ RasterLayer { TileGrid pixels, mask?, opacity, blend, visible, offset }
 └─ Selection { 8-bit mask as TileGrid }
```

- **Tiles:** 64x64, lazily allocated, copy-on-write via shared pointers. Empty tiles cost nothing.
- **Layers** are sparse maps from tile coordinate to tile, with independent bounds. Layers may extend beyond canvas bounds.
- **Masks and selections** reuse the same 8-bit tile grid type, so feather, invert, and painting on masks are ordinary operations.

## Pixel format (decided)
- **Storage:** 8-bit straight (non-premultiplied) RGBA.
- **Compositor math:** premultiplied internally, converted at tile boundaries.
- **Color space:** gamma-encoded values used directly (no linear-light blending), matching the era.
- Do not change this without updating every blend test.

## Compositor
- Walks the stack bottom-up per dirty tile: pixels x mask x opacity, then blend.
- Per-tile composite cache with dirty-rect invalidation. An edit invalidates only affected tiles.
- Runs on a thread pool. UI shows a stale tile until the fresh one is ready. *(Not built: the compositor is synchronous and no milestone lists this.)*

## Undo
- Before a command mutates tiles, snapshot affected tiles (copy-on-write makes this cheap).
- Undo record: map of `(layer, tile coord) -> old tile`, plus non-pixel state (layer list, properties).
- **Ship single-level undo/redo toggle (3.0 behavior).** Keep the design able to grow into a history stack.

## Paint engine
- Dab-based stamping along strokes: spacing, size, hardness, opacity, flow.
- Strokes accumulate in a temporary buffer, then commit through opacity and blend, so overlapping dabs don't double-apply opacity.
- Each tool is a state machine (`press / drag / release / cancel`) that emits one command on release.

## Adjustments and filters
- **Adjustments:** LUT-based where possible (levels, curves, brightness/contrast, invert). Hue/saturation is per-pixel. Applied as direct edits, with preview.
- **Filters:** take a source region plus a halo for kernel radius; cancellable with progress callback. Preview on a downsampled or visible-region copy first.
- **Transform:** 3x3 matrix, inverse mapping, nearest/bilinear while dragging, bicubic on commit.

## File formats
- **Native:** zip containing `manifest.json` (layers, properties, versions) plus per-layer tile blobs (zstd/lz4). Include a format version number from the first write.
- **Import/export:** PNG and JPEG.

## Scope

### Tier 1: must ship
- [x] **1. Canvas (2h):** tiled storage, GL display, zoom/pan
- [x] **2. Layers + compositor (3h):** stack, opacity, visibility, reorder, Normal blend, dirty-tile cache *(core only; the thread pool from the Compositor section is not built)*
- [ ] **3. Blend modes + masks (2h):** Multiply, Screen, Overlay, Soft Light, Hard Light, Darken, Lighten, Difference, Color Dodge, Color Burn; per-layer grayscale masks
- [ ] **4. Commands + undo (2h):** command dispatch, tile-snapshot single-level undo/redo
- [ ] **5. Selection + paint (4h):** marquee, lasso, magic wand, feather, invert; brush, eraser, paint bucket, eyedropper
- [ ] **6. Adjustments (2h):** levels, curves, hue/saturation, brightness/contrast, invert
- [ ] **7. Filters (1.5h):** Gaussian blur, unsharp mask, noise, motion blur, optionally emboss/find edges
- [ ] **8. Transform (2.5h):** crop, canvas/image size, scale/rotate with bicubic resampling *(basic canvas ops already exist)*
- [ ] **9. File I/O (2h):** native save/load, PNG/JPEG import/export
- [ ] **10. Minimal UI (2h):** toolbox, tool options bar, layers palette (thumbnails, eye toggle, drag-reorder)

### Tier 2: only if ahead of schedule
- Adjustment layers (reuse milestone 6 code)
- Skew, perspective, distort
- Alpha channels (save/load selection)
- Clone stamp, smudge, dodge/burn, gradient
- HSL blend modes (Hue, Saturation, Color, Luminosity)
- Grayscale mode
- Python bindings beyond a minimal test harness

### Tier 3: cut for now
- Paths and pen tool
- Actions recorder/player
- Rulers, guides, grid, navigator
- Multi-step history
- PSD import/export
- CMYK, 16-bit, ICC, tablet pressure, layer effects, text

## Testing
- Golden-image tests for every blend mode, filter, and adjustment.
- Property tests: undo then redo yields identical tiles; save then load round-trips.
- Headless integration tests drive the same Commands as the UI (C++ or Python).
- Full suite must pass before every commit.

## Conventions
- One concept per file; keep headers small.
- Commands are fully parameterized and serializable (keeps Actions possible later).
- No raw owning pointers; use `shared_ptr` for tiles and `unique_ptr` elsewhere.
- Commit messages: `m<N>: <what changed>` (for example `m3: add multiply blend + golden test`).

## Definition of done
Open a PNG or JPEG, add layers with blend modes and masks, make selections, paint, apply adjustments and filters, transform, undo the last action, and save and reload in the native format.

## Open decisions
- Native format name and extension
- Eigen vs GLM

## Session log
- Session 1: core `Image`/`Document`/`Viewport`, stb I/O, Catch2 goldens, NanoGUI viewer. Then `TileGrid` (64x64, sparse, copy-on-write) as `Document` storage. Canvas resize/crop/rotate/flip were built early (milestone 8). Viewer confirmed working by eye; milestone 1 closed. Milestone 2: `blend`, `Layer`, `LayerStack`, tile-cached `Compositor`, `Document` moved onto the stack; core is built with `-ffp-contract=off` so blend goldens are CPU-independent.