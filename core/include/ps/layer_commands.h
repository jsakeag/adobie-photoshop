#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "ps/command.h"

namespace ps {

// Commands that change the layer stack or a layer's properties. Layer
// indices run bottom to top; an invalid one makes execute() throw
// std::out_of_range.

// Inserts below the layer currently at `index`; the layer count adds on top.
class AddLayerCommand : public Command {
 public:
  AddLayerCommand(std::size_t index, Layer layer)
      : index(index), layer(std::move(layer)) {}
  std::string name() const override { return "New Layer"; }

  const std::size_t index;
  const Layer layer;

 protected:
  void apply(Document& doc) override { doc.add_layer(index, layer); }
};

class RemoveLayerCommand : public Command {
 public:
  explicit RemoveLayerCommand(std::size_t index) : index(index) {}
  std::string name() const override { return "Delete Layer"; }

  const std::size_t index;

 protected:
  void apply(Document& doc) override { doc.remove_layer(index); }
};

// Moves a layer so that it ends up at index `to`.
class MoveLayerCommand : public Command {
 public:
  MoveLayerCommand(std::size_t from, std::size_t to) : from(from), to(to) {}
  std::string name() const override { return "Layer Order"; }

  const std::size_t from;
  const std::size_t to;

 protected:
  void apply(Document& doc) override { doc.move_layer(from, to); }
};

class SetLayerOpacityCommand : public Command {
 public:
  SetLayerOpacityCommand(std::size_t index, float opacity)
      : index(index), opacity(opacity) {}
  std::string name() const override { return "Layer Opacity"; }

  const std::size_t index;
  const float opacity;  // clamped to 0..1 when applied

 protected:
  void apply(Document& doc) override { doc.set_layer_opacity(index, opacity); }
};

class SetLayerVisibleCommand : public Command {
 public:
  SetLayerVisibleCommand(std::size_t index, bool visible)
      : index(index), visible(visible) {}
  std::string name() const override {
    return visible ? "Show Layer" : "Hide Layer";
  }

  const std::size_t index;
  const bool visible;

 protected:
  void apply(Document& doc) override { doc.set_layer_visible(index, visible); }
};

// Places the layer's pixel (0, 0) at `offset` in the document.
class SetLayerOffsetCommand : public Command {
 public:
  SetLayerOffsetCommand(std::size_t index, Point offset)
      : index(index), offset(offset) {}
  std::string name() const override { return "Move"; }

  const std::size_t index;
  const Point offset;

 protected:
  void apply(Document& doc) override { doc.set_layer_offset(index, offset); }
};

class SetLayerBlendCommand : public Command {
 public:
  SetLayerBlendCommand(std::size_t index, BlendMode blend)
      : index(index), blend(blend) {}
  std::string name() const override { return "Blend Mode"; }

  const std::size_t index;
  const BlendMode blend;

 protected:
  void apply(Document& doc) override { doc.set_layer_blend(index, blend); }
};

// Adds or replaces the layer's mask; std::nullopt removes it.
class SetLayerMaskCommand : public Command {
 public:
  SetLayerMaskCommand(std::size_t index, std::optional<MaskGrid> mask)
      : index(index), mask(std::move(mask)) {}
  std::string name() const override {
    return mask ? "Add Layer Mask" : "Remove Layer Mask";
  }

  const std::size_t index;
  const std::optional<MaskGrid> mask;

 protected:
  void apply(Document& doc) override { doc.set_layer_mask(index, mask); }
};

}  // namespace ps
