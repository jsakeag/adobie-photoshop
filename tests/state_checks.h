#pragma once

#include "ps/compositor.h"
#include "ps/document.h"

// Whether two layers are the same in every property and every pixel.
inline bool same_layer(const ps::Layer& a, const ps::Layer& b) {
  return a.name == b.name && a.offset.x == b.offset.x &&
         a.offset.y == b.offset.y && a.opacity == b.opacity &&
         a.visible == b.visible && a.blend == b.blend && a.pixels == b.pixels &&
         a.mask.has_value() == b.mask.has_value() &&
         (!a.mask || *a.mask == *b.mask);
}

inline bool same_state(const ps::DocumentState& a, const ps::DocumentState& b) {
  if (a.width != b.width || a.height != b.height ||
      a.layers.size() != b.layers.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.layers.size(); ++i) {
    if (!same_layer(a.layers.at(i), b.layers.at(i))) return false;
  }
  return true;
}

// What the document should show, composited from scratch with no cache.
inline ps::Image uncached(const ps::Document& doc) {
  ps::Compositor fresh;
  return fresh.render(doc.layers(), doc.bounds());
}

// The document's cached composite agrees with a fresh one.
inline bool cache_is_current(const ps::Document& doc) {
  return doc.flatten() == uncached(doc);
}
