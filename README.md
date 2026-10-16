# Mock Photoshop

A raster image editor modeled on Photoshop 3.0-4.0, written in C++17. The core is headless; a thin NanoGUI app shows the result. See [CLAUDE.md](CLAUDE.md) for the plan and architecture.

**Status:** milestones 1 (canvas), 2 (layers + compositor), 3 (blend modes + masks), 4 (commands + undo) and 4b (GUI for all of those) are done. There are no painting or selection tools yet (milestone 5).

## Build and test

```
cmake -S . -B build && cmake --build build -j
./build/tests/ps_tests
```

Add `-DPS_BUILD_APP=OFF` to the first command to skip the NanoGUI app.

## Run the app

```
./build/app/mock_photoshop path/to/image.png
```

- **View:** drag to pan, scroll to zoom. Cmd with `=`, `-`, `0`, `1` zooms in, out, fits, or goes to 100%.
- **Open:** Cmd+O, or drop a file on the window. This replaces the document.
- **Undo:** Cmd+Z, or the button at the top of the Layers palette. Undo is single-level as in Photoshop 3: Cmd+Z again redoes.
- **Layers palette** (right side; drag its title bar to move it):
  - The list shows the top layer first. Click a name to select it; the checkbox shows or hides it. `[mask]` marks a layer with a mask.
  - The blend mode menu and opacity slider act on the selected layer.
  - **New** adds an empty layer above the selection, **Delete** removes the selection (not the last layer), **Up**/**Down** reorder.
  - **Open Image as Layer...** adds a file as a new layer, centred on the canvas. This is the way to get a second image in to try blend modes.
  - **Layer mask: Reveal All / Hide All** adds a mask that shows or hides the whole layer; **Remove** deletes it. Painting on masks comes with the paint tools in milestone 5.

To check the GUI without clicking through it, `--selftest` runs the same actions the widgets call, checks the document after each, and saves screenshots:

```
mkdir -p /tmp/shots
./build/app/mock_photoshop --selftest /tmp/shots tests/fixtures/selftest_base.png tests/fixtures/selftest_layer.png
```

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

Layer indices run bottom to top, so the background is 0. `editor.document()` is read-only: `layers()` to inspect, `flatten()` or `render(rect)` for the composite.

Blend modes: Normal, Multiply, Screen, Overlay, Soft Light, Hard Light, Darken, Lighten, Difference, Color Dodge, Color Burn. `tests/golden/blend_*.png` shows what each one does.

Working examples are in [tests/test_editor.cpp](tests/test_editor.cpp), [tests/test_commands.cpp](tests/test_commands.cpp) and [tests/test_layer_mask.cpp](tests/test_layer_mask.cpp).
