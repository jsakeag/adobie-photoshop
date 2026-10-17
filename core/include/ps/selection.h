#pragma once

#include <optional>
#include <vector>

#include "ps/geometry.h"
#include "ps/image.h"
#include "ps/mask_grid.h"

namespace ps {

// Selections are MaskGrids in document coordinates: 255 selected, 0 not,
// values between for feathered edges. No selection (std::nullopt) means
// everything can be edited.

enum class SelectionOp { Replace, Add, Subtract };

// Shapes. A pixel is inside when its centre is.
MaskGrid rect_mask(Rect r);
MaskGrid ellipse_mask(Rect bounds);
// Even-odd rule; fewer than three points select nothing.
MaskGrid polygon_mask(const std::vector<Vec2>& points);

// The contiguous region (4-connected) around `seed` whose pixels differ
// from the seed's by at most `tolerance` in every channel. `pixels` sits at
// document position `origin`; a seed outside it selects nothing.
MaskGrid flood_mask(const Image& pixels, Point origin, Point seed,
                    int tolerance);

// The selection after applying `shape` to `current` with `op`. Adding to no
// selection starts one; subtracting from no selection leaves none.
std::optional<MaskGrid> combine_selection(const std::optional<MaskGrid>& current,
                                          const MaskGrid& shape, SelectionOp op);
MaskGrid inverted(const MaskGrid& mask);
MaskGrid intersected(const MaskGrid& a, const MaskGrid& b);
// Blurs the mask so edges fade over about `radius` pixels each way.
MaskGrid feathered(const MaskGrid& mask, float radius);

// Tight bounds of the pixels inside `within` that are at all selected.
Rect selected_bounds(const MaskGrid& selection, Rect within);

// Draws the selection's edge into `image` (whose top-left is at document
// position `origin`) as black and white dashes, Photoshop's marching ants
// standing still. Edge pixels are selected pixels (value >= 128) with an
// unselected neighbour.
void draw_selection_outline(Image& image, const MaskGrid& selection,
                            Point origin);

}  // namespace ps
