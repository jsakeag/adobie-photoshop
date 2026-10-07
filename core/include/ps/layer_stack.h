#pragma once

#include <cstddef>
#include <vector>

#include "ps/layer.h"

namespace ps {

// Layers ordered bottom to top: index 0 is drawn first.
// Out-of-range indices throw std::out_of_range.
class LayerStack {
 public:
  std::size_t size() const { return layers_.size(); }
  bool empty() const { return layers_.empty(); }

  const Layer& at(std::size_t index) const { return layers_.at(index); }
  Layer& at(std::size_t index) { return layers_.at(index); }

  // Inserts below the layer currently at `index`; size() adds on top.
  void insert(std::size_t index, Layer layer);
  Layer remove(std::size_t index);
  // Moves a layer so that it ends up at index `to`.
  void move(std::size_t from, std::size_t to);

  std::vector<Layer>::const_iterator begin() const { return layers_.begin(); }
  std::vector<Layer>::const_iterator end() const { return layers_.end(); }

 private:
  std::vector<Layer> layers_;
};

}  // namespace ps
