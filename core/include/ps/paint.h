#pragma once

#include <cstddef>

#include "ps/document.h"

namespace ps {

// What painting changes: the layer's pixels or its mask.
enum class PaintTarget { Pixels, Mask };

// Paints `color` onto a layer through `coverage` (document coordinates,
// 255 = full), scaled by `opacity` and limited to the canvas and to the
// document's selection.
//
// On pixels, painting composites the colour over them (Normal) and erasing
// lowers their alpha. On a mask, painting moves values toward the colour's
// grey level (black hides, white shows) and erasing toward white. Throws
// std::logic_error when targeting a mask the layer does not have.
void paint_coverage(Document& doc, std::size_t layer, PaintTarget target,
                    const MaskGrid& coverage, Color color, float opacity,
                    bool erase);

// The colour's grey level, as painting on a mask uses it.
std::uint8_t grey_level(Color c);

}  // namespace ps
