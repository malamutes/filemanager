#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

struct FileMetadata {
    std::uintmax_t file_size;
    fs::file_status status;
    fs::file_time_type last_write;
    bool exists;
};

struct ProcessIDMetadata {
    std::string pid;
    std::string vmrss;
    std::string process_name;
    std::string state;
};

struct BatteryInfo {
    std::string battery;
    std::string status;
    std::string model_name;
};

struct CPUInfo {
    std::string model_name;
    std::string cores;
    std::string threads;
    std::string current_mhz;
};