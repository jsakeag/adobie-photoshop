#include "ps/adjust_commands.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace ps {

void AdjustCommand::apply(Document& doc) {
  const Layer& layer = doc.layers().at(index);
  const Rect area = layer.bounds().intersect(doc.bounds());
  if (area.empty()) return;
  std::vector<std::uint8_t> select(
      static_cast<std::size_t>(area.width) * area.height, 255);
  if (doc.selection()) select = doc.selection()->read(area);

  const auto adjust = adjustment_function(adjustment);
  const Rect local{area.x - layer.offset.x, area.y - layer.offset.y, area.width,
                   area.height};
  TileGrid& pixels = doc.edit_layer_pixels(index, local);
  Image image = pixels.read(local);
  std::uint8_t* p = image.data();
  for (std::size_t i = 0; i < select.size(); ++i, p += Image::kChannels) {
    if (p[3] == 0 || select[i] == 0) continue;
    const Color out = adjust({p[0], p[1], p[2], p[3]});
    const float w = select[i] / 255.0f;
    const std::uint8_t channels[3] = {out.r, out.g, out.b};
    for (int ch = 0; ch < 3; ++ch) {
      p[ch] = static_cast<std::uint8_t>(std::lround(p[ch] + (channels[ch] - p[ch]) * w));
    }
  }
  pixels.write(image, {local.x, local.y});
}

}  // namespace ps
