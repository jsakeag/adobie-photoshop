#pragma once

#include <memory>
#include <string>
#include <utility>

#include "ps/command.h"
#include "ps/document.h"

namespace ps {

// An open document and its undo state: the command layer. Callers read the
// document and change it only by executing commands.
//
// Undo is single-level, as in Photoshop 3: the last command can be undone
// and then redone, and running a new command replaces it.
class Editor {
 public:
  explicit Editor(Document document) : document_(std::move(document)) {}

  const Document& document() const { return document_; }

  // Runs the command and makes it the one that undo() takes back. If the
  // command throws, the document and the undo state are unchanged.
  void execute(std::unique_ptr<Command> command);

  bool can_undo() const { return last_ && !undone_; }
  bool can_redo() const { return last_ && undone_; }
  // Return false, doing nothing, when there is nothing to undo or redo.
  bool undo();
  bool redo();
  // Photoshop 3's Cmd+Z: undoes the last command, or redoes it if it is
  // already undone. Returns false when there is no command.
  bool toggle_undo() { return can_undo() ? undo() : redo(); }

  // Name of the command undo() or redo() would act on; empty if none.
  std::string undo_name() const { return can_undo() ? last_->name() : ""; }
  std::string redo_name() const { return can_redo() ? last_->name() : ""; }

 private:
  Document document_;
  std::unique_ptr<Command> last_;
  bool undone_ = false;
};

}  // namespace ps
