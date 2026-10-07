#include "ps/layer_stack.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ps {

void LayerStack::insert(std::size_t index, Layer layer) {
  if (index > layers_.size()) {
    throw std::out_of_range("LayerStack::insert index out of range");
  }
  layers_.insert(layers_.begin() + index, std::move(layer));
}

Layer LayerStack::remove(std::size_t index) {
  Layer layer = std::move(layers_.at(index));
  layers_.erase(layers_.begin() + index);
  return layer;
}

void LayerStack::move(std::size_t from, std::size_t to) {
  if (from >= layers_.size() || to >= layers_.size()) {
    throw std::out_of_range("LayerStack::move index out of range");
  }
  const auto first = layers_.begin();
  if (from < to) {
    std::rotate(first + from, first + from + 1, first + to + 1);
  } else {
    std::rotate(first + to, first + from, first + from + 1);
  }
}

}  // namespace ps
