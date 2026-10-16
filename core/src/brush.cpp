#include "ps/brush.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ps {

void Stroke::add_point(Vec2 point) {
  if (!started_) {
    started_ = true;
    last_ = point;
    since_dab_ = 0.0;
    dab(point);
    return;
  }
  const double dx = point.x - last_.x, dy = point.y - last_.y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double step =
      std::max(1.0, static_cast<double>(settings_.spacing * settings_.diameter));
  double at = step - since_dab_;  // distance along the segment of the next dab
  for (; at <= length; at += step) {
    dab({last_.x + dx * at / length, last_.y + dy * at / length});
  }
  since_dab_ = length - (at - step);
  last_ = point;
}

void Stroke::dab(Vec2 centre) {
  ++dabs_;
  const double radius = std::max(0.5, settings_.diameter / 2.0);
  const double inner = radius * std::clamp(settings_.hardness, 0.0f, 1.0f);
  const double flow = std::clamp(settings_.flow, 0.0f, 1.0f);
  const int x0 = static_cast<int>(std::floor(centre.x - radius));
  const int y0 = static_cast<int>(std::floor(centre.y - radius));
  const int x1 = static_cast<int>(std::ceil(centre.x + radius));
  const int y1 = static_cast<int>(std::ceil(centre.y + radius));
  const Rect area{x0, y0, x1 - x0, y1 - y0};

  std::vector<std::uint8_t> values = coverage_.read(area);
  for (int y = 0; y < area.height; ++y) {
    for (int x = 0; x < area.width; ++x) {
      const double px = area.x + x + 0.5 - centre.x;
      const double py = area.y + y + 0.5 - centre.y;
      const double d = std::sqrt(px * px + py * py);
      double strength;
      if (d >= radius) {
        continue;
      } else if (d <= inner) {
        strength = 1.0;
      } else {
        const double t = (radius - d) / (radius - inner);
        strength = t * t * (3.0 - 2.0 * t);  // smooth falloff to the rim
      }
      std::uint8_t& v = values[static_cast<std::size_t>(y) * area.width + x];
      const double old = v / 255.0;
      const double added = strength * flow;
      v = static_cast<std::uint8_t>(std::lround((old + added * (1.0 - old)) * 255.0));
    }
  }
  coverage_.write(values, area);
}

}  // namespace ps
