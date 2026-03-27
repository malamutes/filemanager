#include "../../include/types.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

std::string determineFileIcon(const fs::path &FilePath) {
    try {
        fs::file_status s = fs::status(FilePath);
        auto perms = s.permissions();
        if ((perms & fs::perms::owner_read) == fs::perms::none)
            return "\uf023 ";
        if (fs::is_directory(s))
            return fs::is_empty(FilePath) ? "\uf115 " : "\uf07c ";
        std::string ext = FilePath.extension().string();
        for (auto &c : ext)
            c = std::tolower(c);
        if (ext == ".cpp" || ext == ".hpp")
            return "\ue61d ";
        if (ext == ".py")
            return "\ue235 ";
        return "\uf15b ";
    } catch (...) {
        return "\uf023 ";
    }
}

ProcessIDMetadata getProcessInfo(const std::string &pid) {
    std::ifstream statusFile("/proc/" + pid + "/status");
    std::string line;
    std::string name, vmrss, state;
    while (std::getline(statusFile, line)) {
        if (line.find("Name:") == 0)
            name = line.substr(5);
        if (line.find("VmRSS:") == 0)
            vmrss = line.substr(6);
        if (line.find("State:") == 0)
            state = line.substr(6);
    }
    if (!name.empty()) {
        name.erase(0, name.find_first_not_of(" \t"));
        vmrss.erase(0, vmrss.find_first_not_of(" \t"));
        return {pid, vmrss, name, state};
    }
    return {"", "", "", ""};
}

bool is_number(const std::string &s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}
