#pragma once

#include <cstddef>

#include "ps/document.h"

namespace ps {

// A small picture of one layer as it sits on the canvas, for the Layers
// palette: its pixels through its mask, ignoring opacity, visibility and
// blend mode, fitted within max_size x max_size with the canvas's shape.
// Sampled at one point per thumbnail pixel, so it costs the same however
// large the document is.
Image layer_thumbnail(const Document& doc, std::size_t index, int max_size);

}  // namespace ps
