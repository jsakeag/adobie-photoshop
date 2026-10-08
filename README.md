# Mock Photoshop

A raster image editor modeled on Photoshop 3.0-4.0, written in C++17. The core is headless; a thin NanoGUI app shows the result. See [CLAUDE.md](CLAUDE.md) for the plan and architecture.

**Status:** milestones 1 (canvas), 2 (layers + compositor) and 3 (blend modes + masks) are done. Layers exist in the core only: there is no UI to add or edit them yet.

## Build and test

```
cmake -S . -B build && cmake --build build -j
./build/tests/ps_tests
```

Add `-DPS_BUILD_APP=OFF` to the first command to skip the NanoGUI app.

## Run the viewer

```
./build/app/mock_photoshop path/to/image.png
```

Drag to pan, scroll to zoom. Cmd+O opens a file (or drop one on the window); Cmd with `=`, `-`, `0`, `1` zooms in, out, fits, or goes to 100%.

## Using layers (C++)

```cpp
#include "ps/document.h"
#include "ps/image_io.h"
using namespace ps;

Document doc(200, 100);                      // one white "Background" layer

Layer red;
red.name = "Red box";
red.pixels.fill({20, 20, 80, 60}, {255, 0, 0, 255});   // layer coordinates
red.offset = {10, 0};                        // where the layer sits in the document
doc.add_layer(doc.layers().size(), red);     // add on top

doc.set_layer_opacity(1, 0.5f);              // 0..1
doc.set_layer_visible(1, true);
doc.set_layer_blend(1, BlendMode::Multiply);

MaskGrid mask(255);                          // 255 shows, 0 hides; starts all-showing
mask.fill({20, 20, 40, 60}, 0);              // hide the box's left half (layer coordinates)
doc.set_layer_mask(1, mask);
doc.edit_layer_mask(1, {20, 20, 10, 10})     // paint on the mask later
    .fill({20, 20, 10, 10}, 128);

doc.edit_layer_pixels(1, {70, 30, 20, 20})   // declare the area you will change,
    .fill({70, 30, 20, 20}, {0, 0, 255, 255});  // then write to it

save_png(doc.flatten(), "out.png");          // composite of all layers
```

`doc.move_layer(from, to)` reorders and `doc.remove_layer(index)` deletes; indices run bottom to top, so the background is 0.

Change layers only through the `Document` methods. They tell the compositor which tiles are stale; editing `doc.layers()` directly leaves the cache out of date.

Blend modes: Normal, Multiply, Screen, Overlay, Soft Light, Hard Light, Darken, Lighten, Difference, Color Dodge, Color Burn. `tests/golden/blend_*.png` shows what each one does.

Working examples are in [tests/test_document_layers.cpp](tests/test_document_layers.cpp), [tests/test_layer_mask.cpp](tests/test_layer_mask.cpp) and [tests/test_blend.cpp](tests/test_blend.cpp).
