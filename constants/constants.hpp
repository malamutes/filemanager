#pragma once
#include <cstdint>
#include <string>

namespace Filemanager::Constants {
const std::string QUIT = "QUIT";
const std::string BACK = "BACK";
const std::string BATTERY_INFO_PATH = "/sys/class/power_supply/BAT0/";
const std::string CPU_INFO_PATH = "/proc/cpuinfo";
const std::string VIRTUAL_FILE_SYSTEM_PATH = "/proc/";
const std::string ROOT_STORAGE_PATH = "/";
inline constexpr uint32_t GIBIBYTES_CONVERSION = 1073741824;
} // namespace Filemanager::Constants
