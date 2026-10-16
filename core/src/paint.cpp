#include "ps/paint.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "ps/blend.h"

namespace ps {

std::uint8_t grey_level(Color c) {
  return static_cast<std::uint8_t>(
      std::lround(0.299 * c.r + 0.587 * c.g + 0.114 * c.b));
}

void paint_coverage(Document& doc, std::size_t index, PaintTarget target,
                    const MaskGrid& coverage, Color color, float opacity,
                    bool erase) {
  const Layer& layer = doc.layers().at(index);
  if (target == PaintTarget::Mask && !layer.mask) {
    throw std::logic_error("Layer has no mask to paint on");
  }
  const Rect stored =
      coverage.background() == 0 ? coverage.bounds() : doc.bounds();
  const Rect area = stored.intersect(doc.bounds());
  if (area.empty()) return;

  const std::vector<std::uint8_t> cover = coverage.read(area);
  std::vector<std::uint8_t> select(cover.size(), 255);
  if (doc.selection()) select = doc.selection()->read(area);
  const float strength = std::clamp(opacity, 0.0f, 1.0f);
  const auto weight = [&](std::size_t i) {
    return cover[i] / 255.0f * (select[i] / 255.0f) * strength;
  };
  const Rect local{area.x - layer.offset.x, area.y - layer.offset.y, area.width,
                   area.height};

  if (target == PaintTarget::Mask) {
    MaskGrid& mask = doc.edit_layer_mask(index, local);
    std::vector<std::uint8_t> values = mask.read(local);
    const float goal = erase ? 255.0f : grey_level(color);
    for (std::size_t i = 0; i < values.size(); ++i) {
      const float w = weight(i);
      if (w <= 0.0f) continue;
      values[i] = static_cast<std::uint8_t>(
          std::lround(values[i] + (goal - values[i]) * w));
    }
    mask.write(values, local);
    return;
  }

  TileGrid& pixels = doc.edit_layer_pixels(index, local);
  Image image = pixels.read(local);
  const Premul paint = premultiply(color);
  std::uint8_t* p = image.data();
  for (std::size_t i = 0; i < cover.size(); ++i, p += Image::kChannels) {
    const float w = weight(i);
    if (w <= 0.0f) continue;
    if (erase) {
      p[3] = static_cast<std::uint8_t>(std::lround(p[3] * (1.0f - w)));
      if (p[3] == 0) p[0] = p[1] = p[2] = 0;
      continue;
    }
    const Premul src{paint.r * w, paint.g * w, paint.b * w, paint.a * w};
    const Color out = unpremultiply(
        blend(BlendMode::Normal, premultiply({p[0], p[1], p[2], p[3]}), src));
    p[0] = out.r;
    p[1] = out.g;
    p[2] = out.b;
    p[3] = out.a;
  }
  pixels.write(image, {local.x, local.y});
}

}  // namespace ps
