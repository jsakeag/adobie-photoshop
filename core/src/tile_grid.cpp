#include "ps/tile_grid.h"

#include <stdexcept>

namespace ps {

Color TileGrid::pixel(int x, int y) const {
  const auto p = store_.pixel(x, y);
  return {p[0], p[1], p[2], p[3]};
}

void TileGrid::set_pixel(int x, int y, Color c) {
  store_.set_pixel(x, y, {c.r, c.g, c.b, c.a});
}

void TileGrid::fill(Rect r, Color c) { store_.fill(r, {c.r, c.g, c.b, c.a}); }

Image TileGrid::read(Rect r) const {
  if (r.width < 0 || r.height < 0) {
    throw std::invalid_argument("TileGrid::read needs a non-negative size");
  }
  Image out(r.width, r.height);
  store_.read(r, out.data());
  return out;
}

void TileGrid::write(const Image& src, Point dst) {
  store_.write(src.data(), {dst.x, dst.y, src.width(), src.height()});
}

}  // namespace ps
