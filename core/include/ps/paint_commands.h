#pragma once

#include <cstddef>
#include <string>
#include <utility>

#include "ps/command.h"
#include "ps/paint.h"
#include "ps/selection.h"

namespace ps {

// One brush or eraser stroke, as the coverage it built up.
class PaintStrokeCommand : public Command {
 public:
  PaintStrokeCommand(std::size_t index, PaintTarget target, MaskGrid coverage,
                     Color color, float opacity, bool erase)
      : index(index),
        target(target),
        coverage(std::move(coverage)),
        color(color),
        opacity(opacity),
        erase(erase) {}
  std::string name() const override { return erase ? "Eraser" : "Paintbrush"; }

  const std::size_t index;
  const PaintTarget target;
  const MaskGrid coverage;  // document coordinates
  const Color color;
  const float opacity;
  const bool erase;

 protected:
  void apply(Document& doc) override {
    paint_coverage(doc, index, target, coverage, color, opacity, erase);
  }
};

// Fills the contiguous area of similar colour around `seed` (document
// coordinates) on one layer. The area is found from the layer's pixels
// within the canvas; a seed outside the canvas does nothing.
class BucketFillCommand : public Command {
 public:
  BucketFillCommand(std::size_t index, PaintTarget target, Point seed,
                    Color color, int tolerance, float opacity)
      : index(index),
        target(target),
        seed(seed),
        color(color),
        tolerance(tolerance),
        opacity(opacity) {}
  std::string name() const override { return "Paint Bucket"; }

  const std::size_t index;
  const PaintTarget target;
  const Point seed;
  const Color color;
  const int tolerance;  // 0..255 per channel
  const float opacity;

 protected:
  void apply(Document& doc) override {
    const Layer& layer = doc.layers().at(index);
    const Rect canvas = doc.bounds();
    if (!canvas.contains(seed)) return;
    const Image pixels = layer.pixels.read({canvas.x - layer.offset.x,
                                            canvas.y - layer.offset.y,
                                            canvas.width, canvas.height});
    const MaskGrid region = flood_mask(pixels, {canvas.x, canvas.y}, seed, tolerance);
    paint_coverage(doc, index, target, region, color, opacity, false);
  }
};

}  // namespace ps
