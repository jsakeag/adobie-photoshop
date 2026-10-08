#include "ps/command.h"

#include <stdexcept>
#include <utility>

namespace ps {

void Command::execute(Document& doc) {
  if (stage_ != Stage::New) {
    throw std::logic_error("Command::execute called twice");
  }
  saved_ = doc.snapshot();
  try {
    apply(doc);
  } catch (...) {
    doc.restore(saved_);
    saved_ = {};
    throw;
  }
  stage_ = Stage::Done;
}

void Command::undo(Document& doc) {
  if (stage_ != Stage::Done) {
    throw std::logic_error("Command::undo needs an executed command");
  }
  swap_state(doc);
  stage_ = Stage::Undone;
}

void Command::redo(Document& doc) {
  if (stage_ != Stage::Undone) {
    throw std::logic_error("Command::redo needs an undone command");
  }
  swap_state(doc);
  stage_ = Stage::Done;
}

void Command::swap_state(Document& doc) {
  DocumentState current = doc.snapshot();
  doc.restore(saved_);
  saved_ = std::move(current);
}

}  // namespace ps
