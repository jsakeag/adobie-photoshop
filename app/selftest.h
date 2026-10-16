#pragma once

#include <string>

// Drives the app through the same actions its widgets call, checks the
// document after each step and saves a screenshot of each into `out_dir`.
// Returns 0 when every check passes. Needs a display.
int run_selftest(const std::string& out_dir, const std::string& base_image,
                 const std::string& layer_image);
