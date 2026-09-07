#pragma once

#include "Manifest.h"
#include <string>

namespace SE::TOOLS {

bool ReadManifest(const std::string& path, Manifest& out);

} // namespace SE::TOOLS
