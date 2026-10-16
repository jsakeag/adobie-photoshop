#include "ps/tools.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "ps/paint_commands.h"
#include "ps/selection_commands.h"

namespace ps {

namespace {

Point pixel_at(Vec2 p) {
  return {static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))};
}

// What an empty selection gesture means: a click deselects, unless it was
// meant to add to or subtract from the selection.
std::unique_ptr<Command> empty_selection(SelectionOp op) {
  if (op == SelectionOp::Replace) return std::make_unique<DeselectCommand>();
  return nullptr;
}

class MarqueeTool : public Tool {
 public:
  explicit MarqueeTool(bool ellipse) : ellipse_(ellipse) {}

  std::unique_ptr<Command> command() const override {
    if (!active()) return nullptr;
    const Point a = pixel_at(start_), b = pixel_at(end_);
    const Rect area{std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x),
                    std::abs(b.y - a.y)};
    if (area.empty()) return empty_selection(settings().op);
    if (ellipse_) return std::make_unique<SelectEllipseCommand>(area, settings().op);
    return std::make_unique<SelectRectCommand>(area, settings().op);
  }

 protected:
  void begin(Vec2 p) override { start_ = end_ = p; }
  void move(Vec2 p) override { end_ = p; }
  void reset() override {}

 private:
  bool ellipse_;
  Vec2 start_, end_;
};

class LassoTool : public Tool {
 public:
  std::unique_ptr<Command> command() const override {
    if (!active()) return nullptr;
    if (points_.size() < 3) return empty_selection(settings().op);
    return std::make_unique<SelectPolygonCommand>(points_, settings().op);
  }

 protected:
  void begin(Vec2 p) override { points_ = {p}; }
  void move(Vec2 p) override {
    const Vec2& last = points_.back();
    if (last.x != p.x || last.y != p.y) points_.push_back(p);
  }
  void reset() override { points_.clear(); }

 private:
  std::vector<Vec2> points_;
};

class MagicWandTool : public Tool {
 public:
  std::unique_ptr<Command> command() const override {
    if (!active()) return nullptr;
    return std::make_unique<MagicWandCommand>(settings().layer, pixel_at(point_),
                                              settings().tolerance, settings().op);
  }

 protected:
  void begin(Vec2 p) override { point_ = p; }
  void move(Vec2) override {}  // acts where it was pressed
  void reset() override {}

 private:
  Vec2 point_;
};

class BrushTool : public Tool {
 public:
  explicit BrushTool(bool erase) : erase_(erase) {}

  std::unique_ptr<Command> command() const override {
    if (!active() || !stroke_) return nullptr;
    const ToolSettings& s = settings();
    return std::make_unique<PaintStrokeCommand>(s.layer, s.target,
                                                stroke_->coverage(), s.color,
                                                s.brush.opacity, erase_);
  }

 protected:
  void begin(Vec2 p) override {
    stroke_.emplace(settings().brush);
    stroke_->add_point(p);
  }
  void move(Vec2 p) override { stroke_->add_point(p); }
  void reset() override { stroke_.reset(); }

 private:
  bool erase_;
  std::optional<Stroke> stroke_;
};

class BucketTool : public Tool {
 public:
  std::unique_ptr<Command> command() const override {
    if (!active()) return nullptr;
    const ToolSettings& s = settings();
    return std::make_unique<BucketFillCommand>(s.layer, s.target, pixel_at(point_),
                                               s.color, s.tolerance,
                                               s.brush.opacity);
  }

 protected:
  void begin(Vec2 p) override { point_ = p; }
  void move(Vec2) override {}  // acts where it was pressed
  void reset() override {}

 private:
  Vec2 point_;
};

}  // namespace

const char* tool_name(ToolKind kind) {
  switch (kind) {
    case ToolKind::RectMarquee: return "Marquee";
    case ToolKind::EllipseMarquee: return "Ellipse";
    case ToolKind::Lasso: return "Lasso";
    case ToolKind::MagicWand: return "Magic Wand";
    case ToolKind::Brush: return "Brush";
    case ToolKind::Eraser: return "Eraser";
    case ToolKind::Bucket: return "Bucket";
    case ToolKind::Eyedropper: return "Eyedropper";
  }
  return "";
}

void Tool::press(Vec2 point, const ToolSettings& settings) {
  if (active_) cancel();
  settings_ = settings;
  active_ = true;
  begin(point);
}

void Tool::drag(Vec2 point) {
  if (active_) move(point);
}

std::unique_ptr<Command> Tool::release(Vec2 point) {
  if (!active_) return nullptr;
  move(point);
  std::unique_ptr<Command> result = command();
  cancel();
  return result;
}

void Tool::cancel() {
  active_ = false;
  reset();
}

std::unique_ptr<Tool> make_tool(ToolKind kind) {
  switch (kind) {
    case ToolKind::RectMarquee: return std::make_unique<MarqueeTool>(false);
    case ToolKind::EllipseMarquee: return std::make_unique<MarqueeTool>(true);
    case ToolKind::Lasso: return std::make_unique<LassoTool>();
    case ToolKind::MagicWand: return std::make_unique<MagicWandTool>();
    case ToolKind::Brush: return std::make_unique<BrushTool>(false);
    case ToolKind::Eraser: return std::make_unique<BrushTool>(true);
    case ToolKind::Bucket: return std::make_unique<BucketTool>();
    case ToolKind::Eyedropper: return nullptr;
  }
  return nullptr;
}

}  // namespace ps
