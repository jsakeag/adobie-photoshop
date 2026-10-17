# Mock Photoshop

A raster image editor modeled on Photoshop 3.0-4.0, written in C++17. The core is headless; a thin NanoGUI app shows the result. See [CLAUDE.md](CLAUDE.md) for the plan and architecture.

**Status:** milestones 1-9 are done: canvas, layers and compositor, blend modes and masks, commands and undo, selection and paint tools, adjustments, filters, transforms, and saving, all usable from the app. Next is UI polish (milestone 10).

## Build and test

Needs CMake, a C++17 compiler and zstd (`brew install zstd`). Catch2 and NanoGUI are downloaded by CMake; stb, miniz and nlohmann/json are in `third_party/`.

```
cmake -S . -B build && cmake --build build -j
./build/tests/ps_tests
```

Add `-DPS_BUILD_APP=OFF` to the first command to skip the NanoGUI app.

## Run the app

```
./build/app/mock_photoshop path/to/image.png
```

- **View:** scroll to zoom; drag with the right button, or hold Space and drag, to pan. Cmd with `=`, `-`, `0`, `1` zooms in, out, fits, or goes to 100%.
- **File** (buttons at the top of the Tools palette):
  - **Open...** (Cmd+O, or drop a file on the window) opens a `.mps` document or a PNG/JPEG/BMP/TGA/GIF image, replacing the current document.
  - **Save** (Cmd+S) writes the native `.mps` format, keeping layers, masks, blend modes and opacity; **Save As...** (Cmd+Shift+S) picks a new file. A `*` in the title bar means unsaved changes. There is no warning before opening another file over unsaved work.
  - **Export...** (Cmd+E) writes the composite as PNG (with transparency) or JPEG (flattened onto white).
- **Undo:** Cmd+Z, or the button at the top of the Layers palette. Undo is single-level as in Photoshop 3: Cmd+Z again redoes.
- **Layers palette** (right side; drag its title bar to move it):
  - The list shows the top layer first. Click a name to select it; the checkbox shows or hides it. `[mask]` marks a layer with a mask.
  - The blend mode menu and opacity slider act on the selected layer.
  - **New** adds an empty layer above the selection, **Delete** removes the selection (not the last layer), **Up**/**Down** reorder.
  - **Open Image as Layer...** adds a file as a new layer, centred on the canvas. This is the way to get a second image in to try blend modes.
  - **Layer mask: Reveal All / Hide All** adds a mask that shows or hides the whole layer; **Remove** deletes it. To paint on it, choose **Paint on: Mask** in the Tools palette.
- **Tools palette** (left side). Click a tool, then drag on the canvas. Letter keys pick tools as in Photoshop.

  | Tool | Key | What a drag or click does |
  | --- | --- | --- |
  | Marquee / Ellipse | M (press again to switch) | Selects a rectangle or ellipse. A click without dragging deselects. |
  | Lasso | L | Selects the outline you trace. |
  | Magic Wand | W | Selects the connected area of similar colour on the selected layer (Tolerance). |
  | Brush | B | Paints the foreground colour (Size, Hardness, Opacity). |
  | Eraser | E | Erases to transparency, or on a mask paints it back to white. |
  | Bucket | K | Fills the connected area of similar colour (Tolerance, Opacity). |
  | Eyedropper | I | Sets the foreground colour from the picture. |

  - Hold **Shift** while starting a selection to add to the current one, **Option** to subtract from it.
  - Painting and filling only change the selected area when something is selected, and feathered edges paint partly.
  - **Colour** opens a picker for the foreground colour. **Paint on: Layer / Mask** chooses whether tools change the layer's pixels or its mask (black hides, white shows).
  - **Select: All / None / Inverse** (also Cmd+A, Cmd+D, Cmd+Shift+I) and **Feather** with its radius slider.
  - **Image: Rot CW / Rot CCW / Rot 180 / Flip H / Flip V** turn or mirror the whole canvas; **Crop** crops to the selection.
  - Esc cancels a drag in progress. Every stroke, fill and selection change is one undo step.
- **Image palette** (under the Layers palette): pick from the **Adjust...** menu (Levels, Curves, Brightness/Contrast, Hue/Saturation, Invert), the **Filter...** menu (Gaussian Blur, Unsharp Mask, Add Noise, Motion Blur) or the **Transform...** menu (Image Size, Canvas Size, Scale/Rotate Layer). Moving a slider previews the result on the canvas; **Apply** makes it one undo step, **Cancel** drops it. Adjustments and filters change the selected layer, only within the selection if there is one. Curves here are three sliders for the output at inputs 64, 128 and 192.
  - **Image Size** resamples the whole document (aspect kept, bicubic); **Canvas Size** adds or trims canvas around an anchor; **Scale/Rotate Layer** scales and turns the selected layer about its centre (bilinear while previewing, bicubic on Apply).
- **Known issue:** the blend mode menu in the Layers palette opens off the right edge of the window, so it can't be seen. It needs a one-line code fix (see CLAUDE.md).

To check the GUI without clicking through it, `--selftest` runs the same actions the widgets and canvas gestures call, checks the document after each, and saves screenshots:

```
mkdir -p /tmp/shots
./build/app/mock_photoshop --selftest /tmp/shots tests/fixtures/selftest_base.png tests/fixtures/selftest_layer.png
```

## The .mps format

A zip archive you can open with any zip tool:

- `manifest.json`: `format` ("mock-photoshop"), `version` (1), `width`, `height`, and `layers` from bottom to top, each with `name`, `offset`, `opacity`, `visible`, `blend`, `pixels` (the blob's name) and optionally `mask` (`background` value and `tiles` blob).
- `layers/<n>/pixels.zst` and `layers/<n>/mask.zst`: zstd-compressed tiles, each a little-endian int32 tile x and y followed by 64x64 pixels (RGBA, or one byte per mask value).

The selection is not saved. Files from a newer format version are refused rather than misread.

## Editing a document (C++)

Every change is a command run through an `Editor`, which owns the document and its undo state.

```cpp
#include <memory>

#include "ps/editor.h"
#include "ps/image_io.h"
#include "ps/layer_commands.h"
#include "ps/pixel_commands.h"
using namespace ps;

Editor editor(Document(200, 100));           // one white "Background" layer

Layer red;
red.name = "Red box";
red.pixels.fill({20, 20, 80, 60}, {255, 0, 0, 255});   // layer coordinates
red.offset = {10, 0};                        // where the layer sits in the document
editor.execute(std::make_unique<AddLayerCommand>(1, red));   // index 1: above the background

editor.execute(std::make_unique<SetLayerOpacityCommand>(1, 0.5f));
editor.execute(std::make_unique<SetLayerBlendCommand>(1, BlendMode::Multiply));

MaskGrid mask(255);                          // 255 shows, 0 hides; starts all-showing
mask.fill({20, 20, 40, 60}, 0);              // hide the box's left half
editor.execute(std::make_unique<SetLayerMaskCommand>(1, mask));
editor.execute(std::make_unique<FillMaskCommand>(1, Rect{20, 20, 10, 10}, 128));

editor.execute(std::make_unique<FillCommand>(1, Rect{70, 30, 20, 20},
                                             Color{0, 0, 255, 255}));
editor.undo();                               // takes back the blue fill
editor.redo();                               // and brings it back

save_png(editor.document().flatten(), "out.png");   // composite of all layers
```

Undo is single-level, as in Photoshop 3: `undo()` and `redo()` toggle the last command, and a new command replaces it. A command that fails leaves the document untouched.

| Header | Commands |
| --- | --- |
| `ps/layer_commands.h` | `AddLayerCommand`, `RemoveLayerCommand`, `MoveLayerCommand`, `SetLayerOpacityCommand`, `SetLayerVisibleCommand`, `SetLayerOffsetCommand`, `SetLayerBlendCommand`, `SetLayerMaskCommand` |
| `ps/pixel_commands.h` | `FillCommand`, `FillMaskCommand` |
| `ps/canvas_commands.h` | `ResizeCanvasCommand`, `CropCommand`, `RotateCanvasCommand`, `FlipCanvasCommand` |
| `ps/selection_commands.h` | `SelectRectCommand`, `SelectEllipseCommand`, `SelectPolygonCommand`, `MagicWandCommand`, `SelectAllCommand`, `DeselectCommand`, `InvertSelectionCommand`, `FeatherSelectionCommand` |
| `ps/adjust_commands.h` | `AdjustCommand` with a `Levels`, `Curves`, `BrightnessContrast`, `HueSaturation` or `Invert` from `ps/adjustments.h` |
| `ps/filter_commands.h` | `FilterCommand` with a `GaussianBlur`, `UnsharpMask`, `AddNoise` or `MotionBlur` from `ps/filters.h` |
| `ps/transform_commands.h` | `ImageSizeCommand`, `TransformLayerCommand` (a `Matrix3` from `ps/transform.h`); `CropToSelectionCommand` is in `ps/canvas_commands.h` |
| `ps/paint_commands.h` | `PaintStrokeCommand` (coverage from a `Stroke` in `ps/brush.h`), `BucketFillCommand` |

Layer indices run bottom to top, so the background is 0. `editor.document()` is read-only: `layers()` to inspect, `flatten()` or `render(rect)` for the composite.

Blend modes: Normal, Multiply, Screen, Overlay, Soft Light, Hard Light, Darken, Lighten, Difference, Color Dodge, Color Burn. `tests/golden/blend_*.png` shows what each one does.

Working examples are in [tests/test_editor.cpp](tests/test_editor.cpp), [tests/test_commands.cpp](tests/test_commands.cpp) and [tests/test_layer_mask.cpp](tests/test_layer_mask.cpp).
