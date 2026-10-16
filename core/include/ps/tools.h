#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "ps/brush.h"
#include "ps/command.h"
#include "ps/paint.h"
#include "ps/selection.h"

namespace ps {

enum class ToolKind {
  RectMarquee,
  EllipseMarquee,
  Lasso,
  MagicWand,
  Brush,
  Eraser,
  Bucket,
  Eyedropper,
};

inline constexpr std::array<ToolKind, 8> kToolKinds = {
    ToolKind::RectMarquee, ToolKind::EllipseMarquee, ToolKind::Lasso,
    ToolKind::MagicWand,   ToolKind::Brush,          ToolKind::Eraser,
    ToolKind::Bucket,      ToolKind::Eyedropper,
};

const char* tool_name(ToolKind kind);

// Everything a gesture needs to know, captured when it starts.
struct ToolSettings {
  std::size_t layer = 0;
  PaintTarget target = PaintTarget::Pixels;
  Color color = kBlack;
  BrushSettings brush;
  int tolerance = 32;
  SelectionOp op = SelectionOp::Replace;
};

// A tool turns one mouse gesture (press, drags, release) into at most one
// command. Points are in document coordinates.
class Tool {
 public:
  virtual ~Tool() = default;

  void press(Vec2 point, const ToolSettings& settings);
  void drag(Vec2 point);
  // Ends the gesture and returns its command; null if it made none.
  std::unique_ptr<Command> release(Vec2 point);
  // Ends the gesture without a command.
  void cancel();
  bool active() const { return active_; }

  // The command the gesture would make if it ended now, for previews;
  // null if none.
  virtual std::unique_ptr<Command> command() const = 0;

 protected:
  virtual void begin(Vec2 point) = 0;
  virtual void move(Vec2 point) = 0;
  virtual void reset() = 0;
  const ToolSettings& settings() const { return settings_; }

 private:
  ToolSettings settings_;
  bool active_ = false;
};

// The tool for `kind`. The eyedropper only reads a colour and makes no
// command, so it has no Tool: returns null.
std::unique_ptr<Tool> make_tool(ToolKind kind);

}  // namespace ps
