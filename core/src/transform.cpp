#include "ps/transform.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "ps/blend.h"

namespace ps {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Catmull-Rom weights for the four samples around fraction t.
void cubic_weights(double t, double w[4]) {
  const double t2 = t * t, t3 = t2 * t;
  w[0] = -0.5 * t3 + t2 - 0.5 * t;
  w[1] = 1.5 * t3 - 2.5 * t2 + 1.0;
  w[2] = -1.5 * t3 + 2.0 * t2 + 0.5 * t;
  w[3] = 0.5 * t3 - 0.5 * t2;
}

}  // namespace

Matrix3 Matrix3::translate(double x, double y) { return {{1, 0, x, 0, 1, y, 0, 0, 1}}; }

Matrix3 Matrix3::scale(double sx, double sy) { return {{sx, 0, 0, 0, sy, 0, 0, 0, 1}}; }

Matrix3 Matrix3::rotate(double degrees) {
  const double a = degrees * kPi / 180.0;
  const double c = std::cos(a), s = std::sin(a);
  return {{c, -s, 0, s, c, 0, 0, 0, 1}};
}

Matrix3 Matrix3::operator*(const Matrix3& o) const {
  Matrix3 r;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      double sum = 0;
      for (int k = 0; k < 3; ++k) sum += m[i * 3 + k] * o.m[k * 3 + j];
      r.m[i * 3 + j] = sum;
    }
  }
  return r;
}

Vec2 Matrix3::apply(Vec2 p) const {
  const double x = m[0] * p.x + m[1] * p.y + m[2];
  const double y = m[3] * p.x + m[4] * p.y + m[5];
  const double w = m[6] * p.x + m[7] * p.y + m[8];
  return {x / w, y / w};
}

Matrix3 Matrix3::inverse() const {
  const double* a = m;
  const double c00 = a[4] * a[8] - a[5] * a[7];
  const double c01 = a[5] * a[6] - a[3] * a[8];
  const double c02 = a[3] * a[7] - a[4] * a[6];
  const double det = a[0] * c00 + a[1] * c01 + a[2] * c02;
  if (std::fabs(det) < 1e-12) throw std::invalid_argument("Matrix3 is not invertible");
  const double inv = 1.0 / det;
  return {{c00 * inv, (a[2] * a[7] - a[1] * a[8]) * inv, (a[1] * a[5] - a[2] * a[4]) * inv,
           c01 * inv, (a[0] * a[8] - a[2] * a[6]) * inv, (a[2] * a[3] - a[0] * a[5]) * inv,
           c02 * inv, (a[1] * a[6] - a[0] * a[7]) * inv, (a[0] * a[4] - a[1] * a[3]) * inv}};
}

Rect transformed_bounds(const Matrix3& m, Rect r) {
  if (r.empty()) return {};
  const Vec2 corners[4] = {m.apply({double(r.x), double(r.y)}),
                           m.apply({double(r.right()), double(r.y)}),
                           m.apply({double(r.x), double(r.bottom())}),
                           m.apply({double(r.right()), double(r.bottom())})};
  double x0 = corners[0].x, x1 = x0, y0 = corners[0].y, y1 = y0;
  for (const Vec2& c : corners) {
    x0 = std::min(x0, c.x);
    x1 = std::max(x1, c.x);
    y0 = std::min(y0, c.y);
    y1 = std::max(y1, c.y);
  }
  // A hair of tolerance so exact edges do not grow by a pixel.
  const int left = static_cast<int>(std::floor(x0 + 1e-6));
  const int top = static_cast<int>(std::floor(y0 + 1e-6));
  const int right = static_cast<int>(std::ceil(x1 - 1e-6));
  const int bottom = static_cast<int>(std::ceil(y1 - 1e-6));
  return {left, top, right - left, bottom - top};
}

Matrix3 scale_rotate_about(Vec2 centre, double sx, double sy, double degrees) {
  return Matrix3::translate(centre.x, centre.y) * Matrix3::rotate(degrees) *
         Matrix3::scale(sx, sy) * Matrix3::translate(-centre.x, -centre.y);
}

Image transform_image(const Image& source, Point origin, const Matrix3& m,
                      Rect target, Interpolation interpolation) {
  Image out(std::max(0, target.width), std::max(0, target.height));
  if (source.empty() || out.empty()) return out;
  const int w = source.width(), h = source.height();
  std::vector<Premul> src(static_cast<std::size_t>(w) * h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) src[static_cast<std::size_t>(y) * w + x] = premultiply(source.pixel(x, y));
  }
  const auto at = [&](int x, int y) -> const Premul& {
    return src[static_cast<std::size_t>(std::clamp(y, 0, h - 1)) * w + std::clamp(x, 0, w - 1)];
  };
  const Matrix3 back = m.inverse();

  for (int ty = 0; ty < target.height; ++ty) {
    for (int tx = 0; tx < target.width; ++tx) {
      const Vec2 p = back.apply({target.x + tx + 0.5, target.y + ty + 0.5});
      // Continuous position in source pixels, centres at whole numbers.
      const double sx = p.x - origin.x - 0.5, sy = p.y - origin.y - 0.5;
      Premul c;
      if (interpolation == Interpolation::Nearest) {
        c = at(static_cast<int>(std::floor(sx + 0.5)), static_cast<int>(std::floor(sy + 0.5)));
      } else if (interpolation == Interpolation::Bilinear) {
        const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
        const float fx = static_cast<float>(sx - x0), fy = static_cast<float>(sy - y0);
        const Premul& a = at(x0, y0);
        const Premul& b = at(x0 + 1, y0);
        const Premul& d = at(x0, y0 + 1);
        const Premul& e = at(x0 + 1, y0 + 1);
        const auto lerp2 = [&](float p00, float p10, float p01, float p11) {
          return (p00 * (1 - fx) + p10 * fx) * (1 - fy) + (p01 * (1 - fx) + p11 * fx) * fy;
        };
        c = {lerp2(a.r, b.r, d.r, e.r), lerp2(a.g, b.g, d.g, e.g),
             lerp2(a.b, b.b, d.b, e.b), lerp2(a.a, b.a, d.a, e.a)};
      } else {
        const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
        double wx[4], wy[4];
        cubic_weights(sx - x0, wx);
        cubic_weights(sy - y0, wy);
        double r = 0, g = 0, b = 0, a = 0;
        for (int j = 0; j < 4; ++j) {
          for (int i = 0; i < 4; ++i) {
            const double wgt = wx[i] * wy[j];
            const Premul& s = at(x0 - 1 + i, y0 - 1 + j);
            r += s.r * wgt;
            g += s.g * wgt;
            b += s.b * wgt;
            a += s.a * wgt;
          }
        }
        // Catmull-Rom overshoots: keep a valid premultiplied colour.
        const double alpha = std::clamp(a, 0.0, 1.0);
        c = {static_cast<float>(std::clamp(r, 0.0, alpha)),
             static_cast<float>(std::clamp(g, 0.0, alpha)),
             static_cast<float>(std::clamp(b, 0.0, alpha)), static_cast<float>(alpha)};
      }
      out.set_pixel(tx, ty, unpremultiply(c));
    }
  }
  return out;
}

Rect opaque_bounds(const TileGrid& pixels) {
  const Rect area = pixels.bounds();
  if (area.empty()) return {};
  const Image image = pixels.read(area);
  int x0 = area.width, y0 = area.height, x1 = -1, y1 = -1;
  for (int y = 0; y < area.height; ++y) {
    for (int x = 0; x < area.width; ++x) {
      if (image.data()[(static_cast<std::size_t>(y) * area.width + x) * 4 + 3] == 0) continue;
      x0 = std::min(x0, x);
      x1 = std::max(x1, x);
      y0 = std::min(y0, y);
      y1 = std::max(y1, y);
    }
  }
  if (x1 < 0) return {};
  return {area.x + x0, area.y + y0, x1 - x0 + 1, y1 - y0 + 1};
}

}  // namespace ps
