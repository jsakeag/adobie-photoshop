#include "ps/filter_commands.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace ps {

namespace {

Rect grown(Rect r, int by) {
  return {r.x - by, r.y - by, r.width + 2 * by, r.height + 2 * by};
}

Rect moved(Rect r, Point by) { return {r.x + by.x, r.y + by.y, r.width, r.height}; }

}  // namespace

void FilterCommand::apply(Document& doc) {
  const Layer& layer = doc.layers().at(index);
  if (layer.pixels.empty()) return;
  const int halo = filter_halo(filter);
  const Rect area = grown(layer.bounds(), halo).intersect(doc.bounds());
  if (area.empty()) return;

  const Point to_layer{-layer.offset.x, -layer.offset.y};
  const Rect source_area = grown(area, halo);
  const Image source = layer.pixels.read(moved(source_area, to_layer));
  const Rect inner{halo, halo, area.width, area.height};
  const Image filtered =
      apply_filter(filter, source, {source_area.x, source_area.y}).cropped(inner);
  Image result = source.cropped(inner);

  std::vector<std::uint8_t> select(
      static_cast<std::size_t>(area.width) * area.height, 255);
  if (doc.selection()) select = doc.selection()->read(area);
  std::uint8_t* p = result.data();
  const std::uint8_t* f = filtered.data();
  for (std::size_t i = 0; i < select.size(); ++i) {
    const float w = select[i] / 255.0f;
    for (int ch = 0; ch < Image::kChannels; ++ch, ++p, ++f) {
      *p = static_cast<std::uint8_t>(std::lround(*p + (*f - *p) * w));
    }
  }
  const Rect local = moved(area, to_layer);
  doc.edit_layer_pixels(index, local).write(result, {local.x, local.y});
}

}  // namespace ps
