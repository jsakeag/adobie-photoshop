# Mock Photoshop

A raster image editor modeled on Photoshop 3.0-4.0, written in C++17. The core is headless; a thin NanoGUI app shows the result. See [CLAUDE.md](CLAUDE.md) for the plan and architecture.

**Status:** milestones 1 (canvas) and 2 (layers + compositor) are done. Layers exist in the core only: there is no UI to add or edit them yet.

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
doc.move_layer(1, 0);                        // indices run bottom to top

doc.edit_layer_pixels(0, {0, 0, 10, 10})     // declare the area you will change,
    .fill({0, 0, 10, 10}, {0, 0, 255, 255}); // then write to it

save_png(doc.flatten(), "out.png");          // composite of all layers
```

Change layers only through the `Document` methods. They tell the compositor which tiles are stale; editing `doc.layers()` directly leaves the cache out of date. Only Normal blend exists so far.

Working examples are in [tests/test_document_layers.cpp](tests/test_document_layers.cpp) and [tests/test_compositor.cpp](tests/test_compositor.cpp).
