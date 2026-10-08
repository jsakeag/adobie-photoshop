#pragma once

#include <string>

#include "ps/command.h"

namespace ps {

// Commands that change the canvas as a whole. Invalid sizes make execute()
// throw std::invalid_argument.

class ResizeCanvasCommand : public Command {
 public:
  ResizeCanvasCommand(int width, int height, Anchor anchor, Color fill)
      : width(width), height(height), anchor(anchor), fill(fill) {}
  std::string name() const override { return "Canvas Size"; }

  const int width;
  const int height;
  const Anchor anchor;
  const Color fill;

 protected:
  void apply(Document& doc) override {
    doc.resize_canvas(width, height, anchor, fill);
  }
};

class CropCommand : public Command {
 public:
  explicit CropCommand(Rect area) : area(area) {}
  std::string name() const override { return "Crop"; }

  const Rect area;

 protected:
  void apply(Document& doc) override { doc.crop(area); }
};

class RotateCanvasCommand : public Command {
 public:
  explicit RotateCanvasCommand(Rotation rotation) : rotation(rotation) {}
  std::string name() const override { return "Rotate Canvas"; }

  const Rotation rotation;

 protected:
  void apply(Document& doc) override { doc.rotate(rotation); }
};

enum class FlipAxis { Horizontal, Vertical };

class FlipCanvasCommand : public Command {
 public:
  explicit FlipCanvasCommand(FlipAxis axis) : axis(axis) {}
  std::string name() const override { return "Flip Canvas"; }

  const FlipAxis axis;

 protected:
  void apply(Document& doc) override {
    if (axis == FlipAxis::Horizontal) {
      doc.flip_horizontal();
    } else {
      doc.flip_vertical();
    }
  }
};

}  // namespace ps
