#include "ps/selection.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>

namespace ps {

namespace {

using Values = std::vector<std::uint8_t>;

Rect rect_union(Rect a, Rect b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
  const int x1 = std::max(a.right(), b.right());
  const int y1 = std::max(a.bottom(), b.bottom());
  return {x0, y0, x1 - x0, y1 - y0};
}

Rect grown(Rect r, int by) {
  return {r.x - by, r.y - by, r.width + 2 * by, r.height + 2 * by};
}

// Applies `fn` to each pair of values, over everywhere either mask stores
// tiles and over the backgrounds beyond.
MaskGrid zip(const MaskGrid& a, const MaskGrid& b,
             const std::function<std::uint8_t(std::uint8_t, std::uint8_t)>& fn) {
  MaskGrid out(fn(a.background(), b.background()));
  const Rect area = rect_union(a.bounds(), b.bounds());
  if (area.empty()) return out;
  const Values va = a.read(area);
  const Values vb = b.read(area);
  Values result(va.size());
  for (std::size_t i = 0; i < va.size(); ++i) result[i] = fn(va[i], vb[i]);
  out.write(result, area);
  return out;
}

// Writes a 0/255 buffer covering `area` into a fresh mask.
MaskGrid from_values(const Values& values, Rect area) {
  MaskGrid mask(0);
  if (!area.empty()) mask.write(values, area);
  return mask;
}

// One box blur pass of radius `r` along rows (step 1) or columns (step
// width), clamping at the region's edges.
void box_pass(Values& v, int width, int height, int r, bool rows) {
  const int lines = rows ? height : width;
  const int length = rows ? width : height;
  const auto at = [&](int line, int i) -> std::uint8_t& {
    return rows ? v[static_cast<std::size_t>(line) * width + i]
                : v[static_cast<std::size_t>(i) * width + line];
  };
  std::vector<int> source(length);
  const int count = 2 * r + 1;
  for (int line = 0; line < lines; ++line) {
    for (int i = 0; i < length; ++i) source[i] = at(line, i);
    int sum = 0;
    for (int k = -r; k <= r; ++k) sum += source[std::clamp(k, 0, length - 1)];
    for (int i = 0; i < length; ++i) {
      at(line, i) = static_cast<std::uint8_t>((sum + count / 2) / count);
      sum += source[std::min(i + r + 1, length - 1)];
      sum -= source[std::max(i - r, 0)];
    }
  }
}

}  // namespace

MaskGrid rect_mask(Rect r) {
  MaskGrid mask(0);
  mask.fill(r, 255);
  return mask;
}

MaskGrid ellipse_mask(Rect bounds) {
  if (bounds.empty()) return MaskGrid(0);
  const double cx = bounds.x + bounds.width / 2.0;
  const double cy = bounds.y + bounds.height / 2.0;
  const double rx = bounds.width / 2.0, ry = bounds.height / 2.0;
  Values values(static_cast<std::size_t>(bounds.width) * bounds.height);
  for (int y = 0; y < bounds.height; ++y) {
    const double dy = (bounds.y + y + 0.5 - cy) / ry;
    for (int x = 0; x < bounds.width; ++x) {
      const double dx = (bounds.x + x + 0.5 - cx) / rx;
      values[static_cast<std::size_t>(y) * bounds.width + x] =
          dx * dx + dy * dy <= 1.0 ? 255 : 0;
    }
  }
  return from_values(values, bounds);
}

MaskGrid polygon_mask(const std::vector<Vec2>& points) {
  if (points.size() < 3) return MaskGrid(0);
  double x0 = points[0].x, x1 = x0, y0 = points[0].y, y1 = y0;
  for (const Vec2& p : points) {
    x0 = std::min(x0, p.x);
    x1 = std::max(x1, p.x);
    y0 = std::min(y0, p.y);
    y1 = std::max(y1, p.y);
  }
  const Rect area{static_cast<int>(std::floor(x0)), static_cast<int>(std::floor(y0)),
                  static_cast<int>(std::ceil(x1)) - static_cast<int>(std::floor(x0)) + 1,
                  static_cast<int>(std::ceil(y1)) - static_cast<int>(std::floor(y0)) + 1};
  Values values(static_cast<std::size_t>(area.width) * area.height, 0);
  std::vector<double> crossings;
  for (int row = 0; row < area.height; ++row) {
    const double yc = area.y + row + 0.5;
    crossings.clear();
    for (std::size_t i = 0; i < points.size(); ++i) {
      const Vec2& p = points[i];
      const Vec2& q = points[(i + 1) % points.size()];
      if ((p.y <= yc) != (q.y <= yc)) {
        crossings.push_back(p.x + (yc - p.y) * (q.x - p.x) / (q.y - p.y));
      }
    }
    std::sort(crossings.begin(), crossings.end());
    for (std::size_t i = 0; i + 1 < crossings.size(); i += 2) {
      // Pixels whose centres lie in [crossings[i], crossings[i + 1]).
      const int from = static_cast<int>(std::ceil(crossings[i] - 0.5));
      const int to = static_cast<int>(std::ceil(crossings[i + 1] - 0.5));
      for (int x = std::max(from, area.x); x < std::min(to, area.right()); ++x) {
        values[static_cast<std::size_t>(row) * area.width + (x - area.x)] = 255;
      }
    }
  }
  return from_values(values, area);
}

MaskGrid flood_mask(const Image& pixels, Point origin, Point seed,
                    int tolerance) {
  const Point local{seed.x - origin.x, seed.y - origin.y};
  if (!pixels.contains(local.x, local.y)) return MaskGrid(0);
  const int w = pixels.width(), h = pixels.height();
  const Color target = pixels.pixel(local.x, local.y);
  const auto close = [&](const std::uint8_t* p) {
    return std::abs(p[0] - target.r) <= tolerance &&
           std::abs(p[1] - target.g) <= tolerance &&
           std::abs(p[2] - target.b) <= tolerance &&
           std::abs(p[3] - target.a) <= tolerance;
  };

  Values inside(static_cast<std::size_t>(w) * h, 0);
  std::vector<int> stack{local.y * w + local.x};
  inside[stack.back()] = 255;
  int x0 = local.x, x1 = local.x, y0 = local.y, y1 = local.y;
  while (!stack.empty()) {
    const int i = stack.back();
    stack.pop_back();
    const int x = i % w, y = i / w;
    x0 = std::min(x0, x);
    x1 = std::max(x1, x);
    y0 = std::min(y0, y);
    y1 = std::max(y1, y);
    const int neighbours[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
    for (const auto& n : neighbours) {
      if (n[0] < 0 || n[1] < 0 || n[0] >= w || n[1] >= h) continue;
      const int j = n[1] * w + n[0];
      if (inside[j] || !close(pixels.data() + static_cast<std::size_t>(j) * 4)) {
        continue;
      }
      inside[j] = 255;
      stack.push_back(j);
    }
  }

  // Store only the region's bounding box.
  const Rect box{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
  Values values(static_cast<std::size_t>(box.width) * box.height);
  for (int y = 0; y < box.height; ++y) {
    std::copy_n(inside.begin() + (static_cast<std::size_t>(box.y + y) * w + box.x),
                box.width, values.begin() + static_cast<std::size_t>(y) * box.width);
  }
  return from_values(values, {box.x + origin.x, box.y + origin.y, box.width,
                              box.height});
}

std::optional<MaskGrid> combine_selection(const std::optional<MaskGrid>& current,
                                          const MaskGrid& shape, SelectionOp op) {
  switch (op) {
    case SelectionOp::Replace:
      return shape;
    case SelectionOp::Add:
      if (!current) return shape;
      return zip(*current, shape,
                 [](std::uint8_t a, std::uint8_t b) { return std::max(a, b); });
    case SelectionOp::Subtract:
      if (!current) return std::nullopt;
      return zip(*current, shape, [](std::uint8_t a, std::uint8_t b) {
        return std::min<std::uint8_t>(a, static_cast<std::uint8_t>(255 - b));
      });
  }
  return current;
}

MaskGrid inverted(const MaskGrid& mask) {
  return zip(mask, mask, [](std::uint8_t a, std::uint8_t) {
    return static_cast<std::uint8_t>(255 - a);
  });
}

MaskGrid intersected(const MaskGrid& a, const MaskGrid& b) {
  return zip(a, b, [](std::uint8_t x, std::uint8_t y) { return std::min(x, y); });
}

MaskGrid feathered(const MaskGrid& mask, float radius) {
  const Rect bounds = mask.bounds();
  if (radius <= 0.0f || bounds.empty()) return mask;
  // Three box blurs approximate a Gaussian; each spreads the edge by r.
  const int r = std::max(1, static_cast<int>(std::lround(radius / 3.0f)));
  const Rect area = grown(bounds, 3 * r + 1);
  Values values = mask.read(area);
  for (int pass = 0; pass < 3; ++pass) {
    box_pass(values, area.width, area.height, r, true);
    box_pass(values, area.width, area.height, r, false);
  }
  MaskGrid out = mask;
  out.write(values, area);
  return out;
}

void draw_selection_outline(Image& image, const MaskGrid& selection,
                            Point origin) {
  if (image.empty()) return;
  const int w = image.width(), h = image.height();
  // One pixel of margin so edges at the image border see their neighbours.
  const Rect area{origin.x - 1, origin.y - 1, w + 2, h + 2};
  const Values v = selection.read(area);
  const auto selected = [&](int x, int y) {
    return v[static_cast<std::size_t>(y + 1) * area.width + (x + 1)] >= 128;
  };
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!selected(x, y)) continue;
      if (selected(x - 1, y) && selected(x + 1, y) && selected(x, y - 1) &&
          selected(x, y + 1)) {
        continue;
      }
      const bool dark = (((origin.x + x) + (origin.y + y)) & 4) != 0;
      image.set_pixel(x, y, dark ? kBlack : kWhite);
    }
  }
}

}  // namespace ps
