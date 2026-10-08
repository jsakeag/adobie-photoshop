#pragma once

#include <string>

#include "ps/document.h"

namespace ps {

// One edit to a document. Every change goes through a Command so that it can
// be named, undone and (later) recorded.
//
// A subclass holds its parameters as plain values and implements apply().
// Undo comes from the base class: execute() keeps a snapshot of the document
// from before the edit, which shares every tile the edit leaves alone.
class Command {
 public:
  virtual ~Command() = default;

  // Short label for the Edit menu, as in "Undo Fill".
  virtual std::string name() const = 0;

  // Applies the edit. If it throws, the document is left as it was.
  // Throws std::logic_error if called twice.
  void execute(Document& doc);

  // undo() returns the document to its state before execute(); redo()
  // brings the edit back. They alternate, starting with undo(), and throw
  // std::logic_error when called out of turn.
  void undo(Document& doc);
  void redo(Document& doc);

 protected:
  virtual void apply(Document& doc) = 0;

 private:
  enum class Stage { New, Done, Undone };

  // Exchanges the document's state with the saved one.
  void swap_state(Document& doc);

  Stage stage_ = Stage::New;
  DocumentState saved_;  // the state on the other side of the edit
};

}  // namespace ps
