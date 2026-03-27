#pragma once

#include "../../include/types.hpp"
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

std::string determineFileIcon(const fs::path &FilePath);

ProcessIDMetadata getProcessInfo(const std::string &pid);

bool is_number(const std::string &s);