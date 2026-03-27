#pragma once
#include <cstdint>
#include <string>

namespace Filemanager::Constants {
const std::string QUIT_PROGRAM = "QUIT_PROGRAM";
const std::string PARENT_DIRECTORY = "PARENT_DIRECTORY";
const std::string BATTERY_INFO_PATH = "/sys/class/power_supply/BAT0/";
const std::string CPU_INFO_PATH = "/proc/cpuinfo";
const std::string VIRTUAL_FILE_SYSTEM_PATH = "/proc/";
const std::string ROOT_STORAGE_PATH = "/";
const std::string USER_LIST_PATH = "/etc/passwd";
inline constexpr uint32_t GIBIBYTES_CONVERSION = 1073741824;
} // namespace Filemanager::Constants
