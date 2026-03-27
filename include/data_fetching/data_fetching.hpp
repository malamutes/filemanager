#pragma once
#include "../types.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

FileMetadata getFileData(fs::path file_path);

std::string file_type_to_string(fs::file_type ft);

void refeshDirectoryVector(fs::path newCurrentPath, std::vector<std::string> &oldDirectoryVector);

void refeshBatteryInfo(BatteryInfo &BatteryInfo);

void refreshCPUInfo(CPUInfo &cpu);

std::uintmax_t disk_usage_percent(const fs::space_info &si, bool is_privileged = false) noexcept;

void getStorageInformation(fs::space_info &StorageInfo);

void refreshProcessData(std::vector<ProcessIDMetadata> &userProcessIDMetadata, ftxui::ScreenInteractive *screen,
                        std::mutex &refreshSystemData_mutex);

void refreshUserList(std::vector<std::string> &userVector);