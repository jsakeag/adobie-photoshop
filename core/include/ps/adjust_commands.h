#pragma once

#include <cstddef>
#include <string>
#include <utility>

#include "ps/adjustments.h"
#include "ps/command.h"

namespace ps {

// Applies an adjustment to one layer's pixels within the canvas. Where
// something is selected, only the selection changes, and feathered edges
// blend between the old and new colours. Fully transparent pixels are
// left alone.
class AdjustCommand : public Command {
 public:
  AdjustCommand(std::size_t index, Adjustment adjustment)
      : index(index), adjustment(std::move(adjustment)) {}
  std::string name() const override { return adjustment_name(adjustment); }

  const std::size_t index;
  const Adjustment adjustment;

 protected:
  void apply(Document& doc) override;
};

}  // namespace ps
