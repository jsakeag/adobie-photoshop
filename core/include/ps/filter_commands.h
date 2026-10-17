#pragma once

#include <cstddef>
#include <string>
#include <utility>

#include "ps/command.h"
#include "ps/filters.h"

namespace ps {

// Runs a filter over one layer within the canvas. Blurs may spread the
// layer's pixels outward, so the area includes a margin around what the
// layer stores. Where something is selected, only the selection changes
// (feathered edges blend), though the filter still reads its neighbours.
class FilterCommand : public Command {
 public:
  FilterCommand(std::size_t index, Filter filter)
      : index(index), filter(std::move(filter)) {}
  std::string name() const override { return filter_name(filter); }

  const std::size_t index;
  const Filter filter;

 protected:
  void apply(Document& doc) override;
};

}  // namespace ps
