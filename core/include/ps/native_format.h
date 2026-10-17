#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ps/document.h"

namespace ps {

// The native .mps format: a zip holding manifest.json (format, version,
// canvas size, layers bottom to top with their properties) and, per layer,
// zstd-compressed tile blobs for the pixels and the mask. The selection is
// not saved.
inline constexpr int kNativeFormatVersion = 1;
inline constexpr const char* kNativeExtension = ".mps";

// Whether `path` ends in .mps (any case).
bool is_native_path(const std::string& path);

std::vector<std::uint8_t> encode_native(const Document& doc);
// Throws std::runtime_error for anything that is not a valid .mps of this
// version or older.
Document decode_native(const std::vector<std::uint8_t>& bytes);

// Writes to a temporary file beside `path` and then renames it, so a failed
// save leaves any existing file intact. Throw std::runtime_error.
void save_native(const Document& doc, const std::string& path);
Document load_native(const std::string& path);

}  // namespace ps
