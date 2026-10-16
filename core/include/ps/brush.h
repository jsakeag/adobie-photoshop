#pragma once

#include "ps/geometry.h"
#include "ps/mask_grid.h"

namespace ps {

struct BrushSettings {
  float diameter = 20.0f;  // pixels
  float hardness = 0.8f;   // share of the radius at full strength, 0..1
  float opacity = 1.0f;    // the most a stroke can cover, 0..1
  float flow = 1.0f;       // how much each dab adds, 0..1
  float spacing = 0.25f;   // distance between dabs, as a share of diameter
};

// The coverage one stroke builds up, dab by dab, in document coordinates.
//
// Overlapping dabs accumulate up to full coverage; opacity is applied once
// when the stroke is painted, so a stroke never darkens itself beyond it.
class Stroke {
 public:
  explicit Stroke(BrushSettings settings) : settings_(settings) {}

  // The first point stamps a dab; later points stamp dabs along the segment
  // from the previous point, evenly spaced across calls.
  void add_point(Vec2 point);

  const MaskGrid& coverage() const { return coverage_; }
  const BrushSettings& settings() const { return settings_; }
  int dab_count() const { return dabs_; }

 private:
  void dab(Vec2 centre);

  BrushSettings settings_;
  MaskGrid coverage_;
  bool started_ = false;
  Vec2 last_;
  double since_dab_ = 0.0;  // distance travelled since the last dab
  int dabs_ = 0;
};

}  // namespace ps
