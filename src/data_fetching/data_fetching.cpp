#include "../../include/data_fetching/data_fetching.hpp"
#include "../../constants/constants.hpp"
#include "../../include/utils/utils.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <fstream>
#include <vector>

FileMetadata getFileData(fs::path file_path) {
    if (fs::is_directory(file_path) || !fs::exists(file_path)) {
        return {0, fs::file_status(), fs::file_time_type(), false};
    } else {
        return {fs::file_size(file_path), fs::status(file_path), fs::last_write_time(file_path),
                true};
    }
}

std::string file_type_to_string(fs::file_type ft) {
    switch (ft) {
    case fs::file_type::none:
        return "none";
    case fs::file_type::not_found:
        return "not_found";
    case fs::file_type::regular:
        return "regular";
    case fs::file_type::directory:
        return "directory";
    case fs::file_type::symlink:
        return "symlink";
    case fs::file_type::block:
        return "block";
    case fs::file_type::character:
        return "character";
    case fs::file_type::fifo:
        return "fifo";
    case fs::file_type::socket:
        return "socket";
    case fs::file_type::unknown:
        return "unknown";
    default:
        return "invalid";
    }
}

void refeshDirectoryVector(fs::path newCurrentPath, std::vector<std::string> &oldDirectoryVector) {
    oldDirectoryVector.clear();
    oldDirectoryVector.push_back(Filemanager::Constants::QUIT_PROGRAM);
    oldDirectoryVector.push_back(Filemanager::Constants::PARENT_DIRECTORY);
    try {
        for (auto const &dir_entry : std::filesystem::directory_iterator{newCurrentPath}) {
            oldDirectoryVector.push_back(dir_entry.path().filename().string());
        }
    } catch (...) {
    }
}

void refeshBatteryInfo(BatteryInfo &BatteryInfo) {
    std::ifstream capFile(Filemanager::Constants::BATTERY_INFO_PATH + "capacity");
    if (capFile)
        capFile >> BatteryInfo.battery;
    std::ifstream statFile(Filemanager::Constants::BATTERY_INFO_PATH + "status");
    if (statFile)
        statFile >> BatteryInfo.status;
    std::ifstream modelFile(Filemanager::Constants::BATTERY_INFO_PATH + "model_name");
    if (modelFile)
        std::getline(modelFile, BatteryInfo.model_name);
}

void refreshCPUInfo(CPUInfo &cpu) {
    std::ifstream file(Filemanager::Constants::CPU_INFO_PATH);
    std::string line;
    int thread_count = 0;
    while (std::getline(file, line)) {
        if (line.find("processor") == 0)
            thread_count++;
        if (cpu.model_name.empty() && line.find("model name") != std::string::npos) {
            size_t colon = line.find(':');
            if (colon != std::string::npos)
                cpu.model_name = line.substr(colon + 2);
        }
        if (line.find("cpu cores") != std::string::npos) {
            size_t colon = line.find(':');
            if (colon != std::string::npos)
                cpu.cores = line.substr(colon + 2);
        }
        if (line.find("cpu MHz") != std::string::npos) {
            size_t colon = line.find(':');
            if (colon != std::string::npos)
                cpu.current_mhz = line.substr(colon + 2);
        }
    }
    cpu.threads = std::to_string(thread_count);
}

std::uintmax_t disk_usage_percent(const fs::space_info &si, bool is_privileged) noexcept {
    if (constexpr std::uintmax_t X(-1); si.capacity == 0 || si.free == 0 || si.available == 0 ||
                                        si.capacity == X || si.free == X || si.available == X)
        return 100;
    std::uintmax_t unused_space = si.free, capacity = si.capacity;
    if (!is_privileged) {
        const std::uintmax_t privileged_only_space = si.free - si.available;
        unused_space -= privileged_only_space;
        capacity -= privileged_only_space;
    }
    return 100 * (capacity - unused_space) / capacity;
}

void getStorageInformation(fs::space_info &StorageInfo) {
    try {
        StorageInfo = fs::space(Filemanager::Constants::ROOT_STORAGE_PATH);
    } catch (...) {
    }
}

void refreshProcessData(std::vector<ProcessIDMetadata> &userProcessIDMetadata,
                        ftxui::ScreenInteractive *screen, std::mutex &refreshSystemData_mutex) {
    while (true) {
        std::vector<ProcessIDMetadata> temp;
        try {
            for (auto const &dir_entry : std::filesystem::directory_iterator{
                     Filemanager::Constants::VIRTUAL_FILE_SYSTEM_PATH}) {
                if (dir_entry.is_directory()) {
                    std::string pid = dir_entry.path().filename().string();
                    if (is_number(pid)) {
                        auto info = getProcessInfo(pid);
                        if (!info.pid.empty())
                            temp.push_back(info);
                    }
                }
            }
        } catch (...) {
        }
        {
            std::lock_guard<std::mutex> guard(refreshSystemData_mutex);
            userProcessIDMetadata = std::move(temp);
        }
        screen->Post(ftxui::Event::Custom);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
