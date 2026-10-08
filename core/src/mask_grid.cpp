#include "ps/mask_grid.h"

#include <stdexcept>

namespace ps {

namespace {

std::size_t area(Rect r) {
  if (r.width < 0 || r.height < 0) {
    throw std::invalid_argument("MaskGrid needs a non-negative size");
  }
  return static_cast<std::size_t>(r.width) * r.height;
}

}  // namespace

std::vector<std::uint8_t> MaskGrid::read(Rect r) const {
  std::vector<std::uint8_t> out(area(r));
  store_.read(r, out.data());
  return out;
}

void MaskGrid::write(const std::vector<std::uint8_t>& values, Rect r) {
  if (values.size() != area(r)) {
    throw std::invalid_argument("MaskGrid::write size does not match its rect");
  }
  store_.write(values.data(), r);
}

}  // namespace ps
