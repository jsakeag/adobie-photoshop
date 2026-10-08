#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ps/command.h"

namespace ps {

// Commands that change pixel or mask content. Areas are in layer
// coordinates; an invalid layer index makes execute() throw
// std::out_of_range.

// Sets every pixel of `area` on one layer to `color`.
class FillCommand : public Command {
 public:
  FillCommand(std::size_t index, Rect area, Color color)
      : index(index), area(area), color(color) {}
  std::string name() const override { return "Fill"; }

  const std::size_t index;
  const Rect area;
  const Color color;

 protected:
  void apply(Document& doc) override {
    doc.edit_layer_pixels(index, area).fill(area, color);
  }
};

// Sets `area` of a layer's mask to `value` (0 hides, 255 shows).
// execute() throws std::logic_error if the layer has no mask.
class FillMaskCommand : public Command {
 public:
  FillMaskCommand(std::size_t index, Rect area, std::uint8_t value)
      : index(index), area(area), value(value) {}
  std::string name() const override { return "Fill Mask"; }

  const std::size_t index;
  const Rect area;
  const std::uint8_t value;

 protected:
  void apply(Document& doc) override {
    doc.edit_layer_mask(index, area).fill(area, value);
  }
};

}  // namespace ps
