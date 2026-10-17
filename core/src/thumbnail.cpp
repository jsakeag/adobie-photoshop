#include "ps/thumbnail.h"

#include <algorithm>
#include <cmath>

namespace ps {

Image layer_thumbnail(const Document& doc, std::size_t index, int max_size) {
  const Layer& layer = doc.layers().at(index);
  const double scale =
      std::min(1.0, double(max_size) / std::max(doc.width(), doc.height()));
  const int w = std::max(1, static_cast<int>(std::lround(doc.width() * scale)));
  const int h = std::max(1, static_cast<int>(std::lround(doc.height() * scale)));
  Image thumb(w, h);
  if (layer.pixels.empty()) return thumb;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      // The document point at the middle of this thumbnail pixel.
      const int dx = static_cast<int>((x + 0.5) * doc.width() / w);
      const int dy = static_cast<int>((y + 0.5) * doc.height() / h);
      const int lx = dx - layer.offset.x, ly = dy - layer.offset.y;
      Color c = layer.pixels.pixel(lx, ly);
      if (layer.mask) {
        c.a = static_cast<std::uint8_t>((c.a * layer.mask->value(lx, ly) + 127) / 255);
        if (c.a == 0) c = kTransparent;
      }
      thumb.set_pixel(x, y, c);
    }
  }
  return thumb;
}

}  // namespace ps
