#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ps/command.h"
#include "ps/selection.h"

namespace ps {

// Commands that change the selection. Shapes are in document coordinates
// and combine with the current selection by `op`.

class SelectRectCommand : public Command {
 public:
  SelectRectCommand(Rect area, SelectionOp op) : area(area), op(op) {}
  std::string name() const override { return "Rectangular Marquee"; }

  const Rect area;
  const SelectionOp op;

 protected:
  void apply(Document& doc) override {
    doc.set_selection(combine_selection(doc.selection(), rect_mask(area), op));
  }
};

class SelectEllipseCommand : public Command {
 public:
  SelectEllipseCommand(Rect bounds, SelectionOp op) : bounds(bounds), op(op) {}
  std::string name() const override { return "Elliptical Marquee"; }

  const Rect bounds;
  const SelectionOp op;

 protected:
  void apply(Document& doc) override {
    doc.set_selection(
        combine_selection(doc.selection(), ellipse_mask(bounds), op));
  }
};

class SelectPolygonCommand : public Command {
 public:
  SelectPolygonCommand(std::vector<Vec2> points, SelectionOp op)
      : points(std::move(points)), op(op) {}
  std::string name() const override { return "Lasso"; }

  const std::vector<Vec2> points;
  const SelectionOp op;

 protected:
  void apply(Document& doc) override {
    doc.set_selection(
        combine_selection(doc.selection(), polygon_mask(points), op));
  }
};

// Selects the contiguous area of similar colour around `seed` on one layer,
// found within the canvas. A seed outside the canvas changes nothing.
class MagicWandCommand : public Command {
 public:
  MagicWandCommand(std::size_t index, Point seed, int tolerance, SelectionOp op)
      : index(index), seed(seed), tolerance(tolerance), op(op) {}
  std::string name() const override { return "Magic Wand"; }

  const std::size_t index;
  const Point seed;
  const int tolerance;  // 0..255 per channel
  const SelectionOp op;

 protected:
  void apply(Document& doc) override {
    const Layer& layer = doc.layers().at(index);
    const Rect canvas = doc.bounds();
    if (!canvas.contains(seed)) return;
    const Image pixels = layer.pixels.read({canvas.x - layer.offset.x,
                                            canvas.y - layer.offset.y,
                                            canvas.width, canvas.height});
    doc.set_selection(combine_selection(
        doc.selection(), flood_mask(pixels, {canvas.x, canvas.y}, seed, tolerance),
        op));
  }
};

class SelectAllCommand : public Command {
 public:
  std::string name() const override { return "Select All"; }

 protected:
  void apply(Document& doc) override { doc.set_selection(rect_mask(doc.bounds())); }
};

class DeselectCommand : public Command {
 public:
  std::string name() const override { return "Deselect"; }

 protected:
  void apply(Document& doc) override { doc.set_selection(std::nullopt); }
};

// Throws std::logic_error when nothing is selected.
class InvertSelectionCommand : public Command {
 public:
  std::string name() const override { return "Inverse"; }

 protected:
  void apply(Document& doc) override {
    if (!doc.selection()) throw std::logic_error("Nothing is selected");
    doc.set_selection(inverted(*doc.selection()));
  }
};

// Throws std::logic_error when nothing is selected.
class FeatherSelectionCommand : public Command {
 public:
  explicit FeatherSelectionCommand(float radius) : radius(radius) {}
  std::string name() const override { return "Feather"; }

  const float radius;

 protected:
  void apply(Document& doc) override {
    if (!doc.selection()) throw std::logic_error("Nothing is selected");
    doc.set_selection(feathered(*doc.selection(), radius));
  }
};

}  // namespace ps
