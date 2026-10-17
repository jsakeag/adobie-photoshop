# CLAUDE.md: Photoshop 3.0-4.0 Clone

## Project summary
A raster image editor modeled on Photoshop 3.0-4.0 (layers, masks, blend modes, selections, adjustments, filters, transform). Budget: ~25 hours over ~6 weeks, so work in small, test-backed slices and keep this file current.

**Current milestone:** 10 (Polish the UI): not started. All tier 1 features exist; the definition of done can be walked through in the app.
**Next step:** Walk the definition of done by hand in the app and fix what gets in the way. Candidates: the known issues below, an unsaved-changes warning before Open, layer thumbnails, drag-reorder, a drawn Curves editor, palette layout at small window sizes. Keep ~2 hours of buffer unspent.

**Known issues** (left on purpose, 2026-10-16):
- The blend mode menu in the Layers palette opens to the right, off the window's edge, so it can't be seen. Fix: `blend_->set_side(nanogui::Popup::Left)` in `LayersPanel`, as `EffectsPanel` does.
- No warning before Open replaces a document with unsaved changes (the title bar shows `*`).

## Build and test
```
cmake -S . -B build && cmake --build build -j
./build/tests/ps_tests                           # full suite
mkdir -p /tmp/shots && ./build/app/mock_photoshop --selftest /tmp/shots \
  tests/fixtures/selftest_base.png tests/fixtures/selftest_layer.png   # GUI checks + screenshots
PS_UPDATE_GOLDEN=1 ./build/tests/ps_tests "[golden]"   # rewrite goldens, then look at them
cmake -S . -B build -DPS_BUILD_APP=OFF           # core and tests only, skips NanoGUI
```

## Session protocol
1. Start: read this file, run the full test suite, confirm it's green.
2. Work on the current milestone only. Do not start the next one early. **Every milestone includes its GUI binding**: a feature is not done until it can be used from the app, through a command, and the README says how.
3. End: run tests, run `--selftest` and look at its screenshots (macOS screen capture is blocked here; the self-test reads the app's own framebuffer instead). NanoGUI clips a popup to where it was on the previous frame, so capture twice to see an open menu. Extend the self-test with steps for any new GUI. Say plainly what it does not cover: real mouse clicks, popups, file dialogs, key mapping. Update README.md (status line, and usage for anything user-facing that changed; compile any code sample you touch), commit, update "Current milestone" and "Next step" above.
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
  - `TileGrid`: document pixel storage. Sparse and unbounded (negative coordinates allowed), 64x64 tiles allocated on first write, copy-on-write through `shared_ptr`. Missing tiles read as transparent. `read(Rect)` and `write(Image, Point)` exchange flat regions.
  - `MaskGrid`: the 8-bit single-channel grid for layer masks (and selections in milestone 5). Same tiles and copy-on-write, plus a background value that missing tiles read as, so reveal-all (255) and hide-all (0) masks cost nothing. `read`/`write` exchange `std::vector<uint8_t>` regions.
  - `TileStore<N>` (`tile_store.h`): the header-only template both grids wrap; all tile, copy-on-write and background logic lives there. Also defines `kTileSize`, `TileCoord`, `tile_index`, `tile_rect`.
  - `blend`: `BlendMode` (Normal plus the ten milestone 3 modes) and the pure pixel functions: `premultiply`, `unpremultiply`, `blend(mode, dst, src)` on premultiplied float RGBA. A mode is one per-channel function of straight backdrop and source; `blend` applies it only where both pixels have coverage. Soft Light uses Photoshop's formula (square root), not the W3C one.
  - `Layer`: name, `TileGrid` pixels, offset, opacity (0..1), visible, blend, optional `MaskGrid` mask in layer coordinates (it moves with the layer).
  - `LayerStack`: layers bottom to top; insert, remove, move.
  - `Compositor`: flattens a stack per 64x64 document tile, bottom-up (pixels x mask x opacity, then blend), and caches the result. It does not watch layers: callers `invalidate(Rect)` and only those tiles are redone. Single-threaded.
  - `Document`: canvas size, `LayerStack` and its `Compositor`. Layers are read through `layers()` and changed only through `Document` methods (`add_layer`, `remove_layer`, `move_layer`, `set_layer_*` including blend and mask, `edit_layer_pixels(index, area)`, `edit_layer_mask(index, area)`), which invalidate the right tiles. `render(Rect)` and `flatten()` return the composite. Commands are the only intended callers of these methods. `snapshot()` returns a `DocumentState` (size plus layer stack, sharing every tile) and `restore(state)` puts one back, invalidating only tiles whose storage differs (`TileGrid::changed_tiles`). Canvas operations (resize with anchor, crop, rotate, flip) apply to every layer and its mask; layers keep pixels that fall outside the canvas. *(These canvas ops are milestone 8 work done early; don't add more out-of-scope features.)*
  - `selection.h`: selection math on `MaskGrid`s in document coordinates: `rect_mask`, `ellipse_mask`, `polygon_mask` (even-odd), `flood_mask` (contiguous, per-channel tolerance), `combine_selection` (`SelectionOp` Replace/Add/Subtract), `inverted`, `intersected`, `feathered` (three box blurs), `draw_selection_outline` (static marching ants). `Document::selection()` is an optional `MaskGrid` (nullopt = everything editable), part of `DocumentState` so it undoes; canvas operations deselect.
  - `brush.h`: `BrushSettings` (diameter, hardness, opacity, flow, spacing) and `Stroke`, which stamps dabs along points into a coverage `MaskGrid` (flow accumulates, capped at full).
  - `paint.h`: `paint_coverage(doc, layer, PaintTarget, coverage, colour, opacity, erase)`, the one place pixels or masks get painted: coverage x selection x opacity, clipped to the canvas, Normal-composited on pixels or moved toward the colour's grey level on a mask.
  - `adjustments.h`: `Adjustment` variant (`Levels`, `Curves`, `BrightnessContrast`, `HueSaturation`, `Invert`), LUT builders (curves are a monotone cubic through their points, Fritsch-Carlson), `adjust_hue_saturation` (per pixel, via HSL; saturation scales so greys stay grey), `adjustment_function` (pixel to pixel). Brightness/Contrast is the old linear kind: brightness shifts levels, contrast stretches around 127.5.
  - `adjust_commands.h`: `AdjustCommand(layer, adjustment)`: the layer's pixels within the canvas, blended by selection weight, alpha and fully transparent pixels untouched.
  - `filters.h`: `Filter` variant (`GaussianBlur`, `UnsharpMask`, `AddNoise`, `MotionBlur`), `filter_halo` and `apply_filter` on a flat image with a margin. Blurs run on premultiplied float colour (no dark fringes against transparency). Noise is a hash of seed and document position, so preview and apply match exactly.
  - `filter_commands.h`: `FilterCommand(layer, filter)`: the layer's bounds plus the halo, within the canvas, reading a further halo of context; blended by selection weight.
  - `transform.h`: `Matrix3` (translate, scale, rotate clockwise, compose, inverse), `transformed_bounds`, `scale_rotate_about`, `transform_image` (inverse mapping; nearest, bilinear or Catmull-Rom bicubic on premultiplied colour; samples beyond the source repeat its edge, so callers pad with transparency when they want fading edges), `opaque_bounds`.
  - `Document::resize_image` (Image Size: every layer and mask resampled from the canvas area, offsets reset, off-canvas pixels dropped, deselects) and `Document::transform_layer` (one layer and its mask through a matrix). Commands in `transform_commands.h`: `ImageSizeCommand`, `TransformLayerCommand`; `CropToSelectionCommand` in `canvas_commands.h`.
  - `native_format.h`: `.mps` save/load (`encode_native`/`decode_native` in memory, `save_native`/`load_native` on disk, writing to `<path>.saving` then renaming). Zip via vendored miniz; `manifest.json` via vendored nlohmann/json (format "mock-photoshop", `kNativeFormatVersion` = 1, newer versions refused); one zstd blob per layer's pixels and mask, as records of tile x, y and raw tile bytes. Selection not saved. `TileStore::tile_coords()` enumerates tiles for it.
  - `image_io` also has `save_jpeg` (flattened onto white) and `save_image` (PNG or JPEG by extension).
  - `tools.h`: `Tool` state machines (press, drag, release, cancel) for every `ToolKind` but the eyedropper; each makes one command on release, and `command()` gives the in-progress command for previews. `ToolSettings` is captured at press.
  - `Command`: base class for every edit. Subclasses hold their parameters as public const values and implement `name()` and `apply(doc)`. The base class provides `execute`, `undo` and `redo`: `execute` snapshots the document first and restores it if `apply` throws; undo and redo swap the saved snapshot with the current state.
  - `layer_commands.h`, `pixel_commands.h`, `canvas_commands.h`, `selection_commands.h`, `paint_commands.h`: the concrete commands, header-only.
  - `Editor`: the command layer. Owns the `Document`, exposes it read-only, runs commands and holds the single undo step (`execute`, `undo`, `redo`, `can_*`, `*_name`).
  - `Viewport`: zoom/pan math, `screen = doc * zoom + offset`, zoom limited to 1:16 to 16:1.
  - `image_io`: load (PNG/JPEG/BMP/TGA/GIF) and PNG save through stb (compiled static; NanoGUI bundles its own copy). *(Overlaps milestone 9; it exists because the viewer and golden tests need it. JPEG export and the native format are still milestone 9.)*
- `app/` (`mock_photoshop`): NanoGUI shell. No editing logic here.
  - **The GUI exists and grows with the core.** Each milestone adds its controls (see Scope). Widgets only read `editor.document()` and issue commands; anything with logic goes in `core/` so it can be tested headless.
  - `App` (`app.h`): the main window. Holds the `Editor`, the selected layer index and the tool state (UI state, not document state), and the document's name, `.mps` path and a change counter for the title bar's `*`. `open` reads `.mps` or images; `save_to`, `export_to` and the Save/Save As/Export dialogs. Its public actions (`open_as_layer`, `set_blend`, `add_mask`, `toggle_undo`, ...) are what widgets and shortcuts call; each builds a command and runs it, showing errors in the status bar. Widgets are rebuilt in `refresh()` from `draw_all`, never inside their own callbacks.
  - `CanvasView`: draws through `nanogui::ImageView` but takes zoom and pan from `ps::Viewport`. Left-button gestures go to a `PointerHandler` in document coordinates; right-drag or Space+drag pans. Re-uploads the whole flattened composite (plus selection outline) after each change; per-tile upload can come later if it is slow.
  - `ToolsPanel`: the Tools palette window: File buttons (Open, Save, Save As, Export), tool buttons, foreground colour picker, Size/Hardness/Opacity/Tolerance sliders, Paint on Layer/Mask, Select All/None/Inverse, feather radius and Feather, and Image buttons (rotate, flip, crop to selection).
  - `effect.h`: `Effect`, the Image palette's flat variant of every adjustment, filter and transform (`ImageSizeEffect`, `CanvasSizeEffect`, `LayerTransformEffect` hold palette-friendly percents), `kEffectMenus`, `default_effect`, `effect_name`, and `effect_command(layer, effect, doc, preview)`, which builds the real command (bilinear when previewing, bicubic on Apply).
  - `EffectsPanel`: the Image palette (docked under Layers): Adjust..., Filter... and Transform... menus (opening to the left, over the canvas), sliders per kind (Curves as three output sliders at 64/128/192, not a drawn curve), Apply/Cancel. Sliders are rebuilt only when the kind changes.
  - `App` also owns the current `Tool` and its `ToolSettings`, and a pending (previewed) `Effect`. A tool press drops a pending effect. Each applied Add Noise gets a new seed. While an effect is pending or a gesture is active, `show_canvas` runs that command on a copy of the document and shows the copy, so previews use the same code as the real edit.
  - `LayersPanel`: the Layers palette window. Undo/redo toggle, blend mode, opacity (command on slider release), layer rows (top first: visibility checkbox, select button), New/Delete/Up/Down, Open Image as Layer, mask Reveal All/Hide All/Remove.
  - `selftest.cpp`: `--selftest out_dir base.png layer.png` drives `App`'s actions, checks the document and saves screenshots via `App::capture` (reads the back buffer in `draw_teardown`). Fixtures in `tests/fixtures/`.
- `tests/`: Catch2 v3. Golden PNGs in `tests/golden/`, compared exactly; mismatches are written to `build/tests/output/`.
- `third_party/`: vendored stb (image I/O), miniz 3.0.2 (zip) and nlohmann/json 3.11.3. zstd comes from the system via pkg-config (`brew install zstd`). Catch2 and NanoGUI come through FetchContent.

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
7. **A feature without a GUI binding is unfinished.** Core-only work needs a stated reason (for example, a pure math helper).

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
- Runs on a thread pool. UI shows a stale tile until the fresh one is ready. *(Deferred, decided 2026-10-08: the compositor stays synchronous until the UI needs it. Do not build it before then.)*

## Undo
- Before a command mutates tiles, snapshot affected tiles (copy-on-write makes this cheap).
- Undo record: map of `(layer, tile coord) -> old tile`, plus non-pixel state (layer list, properties).
- **Ship single-level undo/redo toggle (3.0 behavior).** Keep the design able to grow into a history stack.
- *As built (milestone 4):* the undo record is a whole `DocumentState` snapshot kept by the command, not a per-command map of affected tiles. Copy-on-write means it still only holds on to the tiles that changed, and every command gets correct undo without writing any. Cost: one pointer copy per tile in the document per command. A history stack is a vector of commands in `Editor` instead of one.
- *Not built:* command serialization. Commands hold plain parameter values so they can be serialized later; nothing reads or writes them yet (needed for Actions, tier 3).

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
- [x] **2. Layers + compositor (3h):** stack, opacity, visibility, reorder, Normal blend, dirty-tile cache *(core only; thread pool deferred, see Compositor)*
- [x] **3. Blend modes + masks (2h):** Multiply, Screen, Overlay, Soft Light, Hard Light, Darken, Lighten, Difference, Color Dodge, Color Burn; per-layer grayscale masks
- [x] **4. Commands + undo (2h):** command dispatch, tile-snapshot single-level undo/redo
- [x] **4b. GUI catch-up (1.5h):** undo/redo, layers panel (visibility, select, opacity, blend mode, new/delete/reorder, mask add/remove, open as layer). Taken from milestone 10's time; each later milestone budgets its own GUI work inside its estimate.
- [x] **5. Selection + paint (4h):** marquee, lasso, magic wand, feather, invert; brush, eraser, paint bucket, eyedropper
- [x] **6. Adjustments (2h):** levels, curves, hue/saturation, brightness/contrast, invert
- [x] **7. Filters (1.5h):** Gaussian blur, unsharp mask, noise, motion blur, optionally emboss/find edges
- [x] **8. Transform (2.5h):** crop, canvas/image size, scale/rotate with bicubic resampling *(scale/rotate by sliders on a whole layer, not on-canvas handles or a selection; no pre-blur when shrinking, so large reductions can alias)*
- [x] **9. File I/O (2h):** native save/load, PNG/JPEG import/export
- [ ] **10. Polish the UI (1h):** *(reduced: GUI now built per milestone)* layer thumbnails, drag-reorder, shortcuts, layout. The panels and tools themselves arrive with milestones 4b-9.

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
- Eigen vs GLM

## Session log
- Session 1: core `Image`/`Document`/`Viewport`, stb I/O, Catch2 goldens, NanoGUI viewer. Then `TileGrid` (64x64, sparse, copy-on-write) as `Document` storage. Canvas resize/crop/rotate/flip were built early (milestone 8). Viewer confirmed working by eye; milestone 1 closed. Milestone 2: `blend`, `Layer`, `LayerStack`, tile-cached `Compositor`, `Document` moved onto the stack; core is built with `-ffp-contract=off` so blend goldens are CPU-independent.
- Session 2 (2026-10-08): README added. Milestone 3: ten blend modes with value tests and a golden each; `TileStore<N>` extracted so `MaskGrid` shares the tile code; layer masks in `Layer`, `Compositor` and `Document`. Thread pool deferred. Milestone 4: `Command` base with snapshot undo, concrete commands for every `Document` mutator, `Editor` with single-level undo/redo, `Document::snapshot`/`restore`; the app now holds an `Editor`.
- Session 2 decision (2026-10-08): GUI pulled forward. Milestone 10 shrinks to polish; milestone 4b binds milestones 2-4 to the GUI; later milestones include their own controls. No GUI code written yet.
- Session 3 (2026-10-16): milestone 4b. Layers palette and Cmd+Z toggle in the app, `blend_mode_name`/`kBlendModes` and `Editor::toggle_undo` in core, `--selftest` with framebuffer screenshots (all checks pass, screenshots inspected).
- Session 3 (2026-10-16), milestone 5: selection math, `Stroke` and `paint_coverage`, selection and paint commands, tool state machines, Tools palette, canvas gestures with live preview, marching ants, shortcuts. Nothing cut. Self-test extended to 32 checks; screenshots inspected.
- Session 3 (2026-10-16), milestone 6: adjustments core (LUTs, monotone curves, HSL), `AdjustCommand`, six goldens, Adjustments palette with live preview, Apply/Cancel. Curves GUI simplified to three sliders (a drawn curve editor is milestone 10 polish if wanted). Self-test at 40 checks; screenshots inspected.
- Session 3 (2026-10-16), milestone 7: four filters with goldens, `FilterCommand`, the Adjustments palette widened to Adjustments & Filters with Adjust... and Filter... menus opening left (fixes the off-screen adjustment menu too). Emboss/find edges skipped. Found while testing: an open popup draws empty on its first frame (clip from the previous frame), so the self-test captures open menus twice. Self-test at 44 checks; screenshots inspected.
- Session 3 (2026-10-16), milestone 8: `Matrix3`/`transform_image` (nearest, bilinear, bicubic), Image Size, layer Scale/Rotate (with mask), Crop to Selection, Transform menu and canvas buttons in the GUI. Self-test at 53 checks; screenshots inspected.
- Session 3 (2026-10-16), milestone 9: `.mps` native format (zip + JSON manifest + zstd tile blobs; extension `.mps` chosen 2026-10-16, user did not object), JPEG export, Open/Save/Save As/Export in the GUI with an unsaved-changes `*`. Self-test at 62 checks; screenshots inspected; the saved file was also checked with unzip and zstd.
