#pragma once

#include "ps/geometry.h"
#include "ps/image.h"
#include "ps/mask_grid.h"
#include "ps/tile_grid.h"

namespace ps {

// A 2D affine or projective transform acting on (x, y, 1), row-major.
struct Matrix3 {
  double m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

  static Matrix3 translate(double x, double y);
  static Matrix3 scale(double sx, double sy);
  // Clockwise on screen (y points down), in degrees.
  static Matrix3 rotate(double degrees);

  Matrix3 operator*(const Matrix3& o) const;  // apply o first, then this
  Vec2 apply(Vec2 p) const;
  // Throws std::invalid_argument when the matrix cannot be inverted.
  Matrix3 inverse() const;
};

enum class Interpolation { Nearest, Bilinear, Bicubic };

// The smallest whole-pixel rectangle holding `r` after `m`.
Rect transformed_bounds(const Matrix3& m, Rect r);

// Scale, then rotate (clockwise, degrees), about `centre`.
Matrix3 scale_rotate_about(Vec2 centre, double sx, double sy, double degrees);

// Fills `target` (document coordinates) by mapping each pixel centre back
// through `m` into `source`, whose top-left is at document position
// `origin`, and sampling there. Samples beyond the source repeat its edge
// pixels: pad the source when the surroundings should be transparent.
// Blends in premultiplied colour.
Image transform_image(const Image& source, Point origin, const Matrix3& m,
                      Rect target, Interpolation interpolation);

// Tight bounds of the pixels that are not fully transparent, in the grid's
// coordinates; empty if there are none.
Rect opaque_bounds(const TileGrid& pixels);

}  // namespace ps
