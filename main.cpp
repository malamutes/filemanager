#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <inttypes.h>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

const std::string QUIT = "QUIT";
const std::string BACK = "BACK";
const std::string BATTERY_INFO_PATH = "/sys/class/power_supply/BAT0/";
const std::string CPU_INFO_PATH = "/proc/cpuinfo";
const std::string ROOT_STORAGE_PATH = "/";
const uint32_t GIBIBYTES_CONVERSION = 1073741824;
using namespace ftxui;
namespace fs = std::filesystem;

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

struct FileMetadata {
    std::uintmax_t file_size;
    fs::file_status file_status;
    fs::file_time_type file_time_type;
    bool existsOrFile;
};

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
    oldDirectoryVector.push_back(QUIT);
    oldDirectoryVector.push_back(BACK);
    for (auto const &dir_entry : std::filesystem::directory_iterator{newCurrentPath}) {
        oldDirectoryVector.push_back(dir_entry.path().filename().string());
    }
}

void refeshBatteryInfo(BatteryInfo &BatteryInfo) {
    std::ifstream capFile(BATTERY_INFO_PATH + "capacity");
    if (capFile)
        capFile >> BatteryInfo.battery;
    std::ifstream statFile(BATTERY_INFO_PATH + "status");
    if (statFile)
        statFile >> BatteryInfo.status;
    std::ifstream modelFile(BATTERY_INFO_PATH + "model_name");
    if (modelFile)
        std::getline(modelFile, BatteryInfo.model_name);
}

void refreshCPUInfo(CPUInfo &cpu) {
    std::ifstream file(CPU_INFO_PATH);
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

std::uintmax_t disk_usage_percent(const fs::space_info &si, bool is_privileged = false) noexcept {
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
    StorageInfo = fs::space(ROOT_STORAGE_PATH);
}

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

int main() {
    auto screen = ScreenInteractive::TerminalOutput();
    fs::path currentPath = fs::current_path();
    std::vector<std::string> currentDirectoryVector = {};
    std::vector<std::string> fileSearchResultsVector = {};
    std::string fileMetadataString = currentPath.string();
    std::string fileAbsolutePath = "";
    std::string searchQuery = "";
    bool isFileFocused = false;
    bool showCreateModal = false;
    bool showRenameModal = false;
    std::string newFileName = "";
    std::string renameInputStr = "";
    BatteryInfo currentBattery;
    CPUInfo currentCPU;
    fs::space_info currentStorage;

    refeshDirectoryVector(currentPath, currentDirectoryVector);

    int selectedIndex = 0;
    int selectedFileResultsIndex = 0;

    MenuOption option;
    option.entries_option.transform = [&](const EntryState &state) {
        std::string label = state.label;
        Element e = text(label);
        if (label != QUIT && label != BACK) {
            std::string icon = determineFileIcon(currentPath / label);
            e = hbox({text(icon) | color(Color::Yellow), text(label)});
        }
        if (state.focused)
            e |= inverted;
        if (state.active)
            e |= bold;
        return e;
    };

    auto menu = Menu(&currentDirectoryVector, &selectedIndex, option);
    auto fileSearchResultsMenu = Menu(&fileSearchResultsVector, &selectedFileResultsIndex);
    auto fileSearchInput = Input(&searchQuery, "Type filename...");

    auto btn_open_vs_file = Button(
        "Open File in VS Code", [&] { system(("code \"" + fileAbsolutePath + "\"").c_str()); },
        ButtonOption::Animated());
    auto btn_open_vs_dir = Button(
        "Open Dir in VS Code", [&] { system(("code \"" + currentPath.string() + "\"").c_str()); },
        ButtonOption::Animated());
    auto btn_delete_file = Button(
        "Delete File",
        [&] {
            if (fs::exists(fileAbsolutePath)) {
                fs::remove(fileAbsolutePath);
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
                menu->TakeFocus();
            }
        },
        ButtonOption::Animated());
    auto btn_create_file =
        Button("Create New File", [&] { showCreateModal = true; }, ButtonOption::Animated());
    auto btn_rename_file = Button(
        "Rename File",
        [&] {
            renameInputStr = fs::path(fileAbsolutePath).filename().string();
            showRenameModal = true;
        },
        ButtonOption::Animated());

    auto fileActions = Container::Vertical({btn_open_vs_file, btn_rename_file, btn_delete_file});
    auto dirActions = Container::Vertical({btn_open_vs_dir, btn_create_file});

    auto fileNameInput = Input(&newFileName, "filename.txt");
    auto fileNameSubmit = Button(
        "Create",
        [&] {
            if (!newFileName.empty()) {
                std::ofstream(currentPath / newFileName).close();
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                showCreateModal = false;
                newFileName = "";
                menu->TakeFocus();
            }
        },
        ButtonOption::Animated());

    auto renameInputComp = Input(&renameInputStr, "");
    auto renameSubmit = Button(
        "Rename",
        [&] {
            if (!renameInputStr.empty()) {
                fs::path oldPath = fileAbsolutePath;
                fs::path newPath = oldPath.parent_path() / renameInputStr;
                fs::rename(oldPath, newPath);
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                showRenameModal = false;
                isFileFocused = false;
                menu->TakeFocus();
            }
        },
        ButtonOption::Animated());

    auto create_modal_container = Container::Vertical({fileNameInput, fileNameSubmit});
    auto rename_modal_container = Container::Vertical({renameInputComp, renameSubmit});

    auto browseContent =
        Container::Horizontal({menu, Container::Vertical({fileActions, dirActions})});
    auto browseRenderer = Renderer(browseContent, [&] {
        auto title = isFileFocused ? " [FILE ACTIONS] " : " [DIR ACTIONS] ";
        auto active_content = isFileFocused ? fileActions->Render() : dirActions->Render();
        return hbox({menu->Render() | flex, separator(),
                     vbox({text(title) | bold | center | color(Color::Yellow), separator(),
                           active_content}) |
                         size(WIDTH, EQUAL, 25)});
    });

    auto searchContent = Container::Vertical({fileSearchInput, fileSearchResultsMenu});
    auto searchRenderer = Renderer(searchContent, [&] {
        return vbox({text("Search in: " + currentPath.string()) | dim,
                     fileSearchInput->Render() | border, separator(),
                     fileSearchResultsMenu->Render() | vscroll_indicator | frame | flex});
    });

    auto systemInformationContent = Container::Vertical({
        Renderer([&] {
            refeshBatteryInfo(currentBattery);
            refreshCPUInfo(currentCPU);
            getStorageInformation(currentStorage);
            float bat_ratio =
                std::stof(currentBattery.battery.empty() ? "0" : currentBattery.battery) / 100.0f;
            float storage_ratio = (float)(currentStorage.capacity - currentStorage.available) /
                                  (float)currentStorage.capacity;
            return vbox({
                hbox({
                    vbox({
                        text("  SYSTEM ") | bold | color(Color::Blue),
                        hbox({text("  OS Kernel:  "), text("Linux") | color(Color::Green)}),
                        hbox({text("  CPU Model:  "), text(currentCPU.model_name) | dim}),
                        hbox({text("  Topology:   "), text(currentCPU.cores + " Cores / " +
                                                           currentCPU.threads + " Threads")}),
                        hbox({text("  Clock:      "),
                              text(currentCPU.current_mhz + " MHz") | color(Color::Yellow)}),
                    }) | flex,
                    separator(),
                    vbox({
                        text("  SESSION ") | bold | color(Color::Blue),
                        hbox({text("  Files:      "),
                              text(std::to_string(currentDirectoryVector.size() - 2))}),
                        hbox({text("  Path:       "),
                              text(currentPath.filename().string()) | color(Color::Yellow)}),
                    }) | size(WIDTH, EQUAL, 30),
                }),
                separatorDouble(),
                hbox({
                    vbox({
                        text(" STORAGE USAGE ") | bold | color(Color::Magenta),
                        hbox({gauge(storage_ratio) | color(Color::Magenta) | border,
                              vbox({text(" " +
                                         std::to_string(
                                             (currentStorage.capacity - currentStorage.available) /
                                             GIBIBYTES_CONVERSION) +
                                         " GB"),
                                    text(" " + std::to_string((int)(storage_ratio * 100)) +
                                         "% Used") |
                                        dim})}),
                    }) | flex,
                    separator(),
                    vbox({
                        text(" POWER STATUS ") | bold | color(Color::Cyan),
                        hbox({gauge(bat_ratio) |
                                  color(currentBattery.status == "Charging" ? Color::Green
                                                                            : Color::Cyan) |
                                  border,
                              vbox({text(" " + currentBattery.battery + "%"),
                                    text(" " + currentBattery.status) |
                                        color(currentBattery.status == "Charging"
                                                  ? Color::Green
                                                  : Color::White)})}),
                    }) | flex,
                }),
            });
        }),
    });

    auto systemInformationRenderer = Renderer(systemInformationContent, [&] {
        return vbox({text("SYSTEM INFORMATION") | bold | center, separator(),
                     systemInformationContent->Render()}) |
               border;
    });

    int main_tab_selected = 0;
    std::vector<std::string> tab_titles = {"Browse Files", "Search Files", "System Information"};
    auto tab_toggle = Toggle(&tab_titles, &main_tab_selected);
    auto main_tabs = Container::Tab({browseRenderer, searchRenderer, systemInformationRenderer},
                                    &main_tab_selected);

    auto main_renderer = Renderer(Container::Vertical({tab_toggle, main_tabs}), [&] {
        auto base_ui = vbox({tab_toggle->Render() | center, separator(), main_tabs->Render() | flex,
                             separator(), text(fileMetadataString) | dim}) |
                       border;
        if (showCreateModal) {
            return dbox(
                {base_ui | dim,
                 vbox({text("Enter New File Name:") | bold | center,
                       fileNameInput->Render() | border, fileNameSubmit->Render() | center}) |
                     border | center | size(WIDTH, GREATER_THAN, 40) | bgcolor(Color::Black)});
        }
        if (showRenameModal) {
            return dbox(
                {base_ui | dim,
                 vbox({text("Rename File To:") | bold | center, renameInputComp->Render() | border,
                       renameSubmit->Render() | center}) |
                     border | center | size(WIDTH, GREATER_THAN, 40) | bgcolor(Color::Black)});
        }
        return base_ui;
    });

    main_renderer |= CatchEvent([&](Event event) -> bool {
        if (showCreateModal) {
            if (event == Event::Escape) {
                showCreateModal = false;
                menu->TakeFocus();
                return true;
            }
            return create_modal_container->OnEvent(event);
        }
        if (showRenameModal) {
            if (event == Event::Escape) {
                showRenameModal = false;
                menu->TakeFocus();
                return true;
            }
            return rename_modal_container->OnEvent(event);
        }
        if (main_tab_selected == 1) {
            if (event == Event::Return && !fileSearchResultsMenu->Focused()) {
                fileSearchResultsVector.clear();
                for (auto const &dir_entry : fs::recursive_directory_iterator(currentPath)) {
                    if (dir_entry.path().filename().string().find(searchQuery) != std::string::npos)
                        fileSearchResultsVector.push_back(dir_entry.path().string());
                }
                return true;
            } else if (event == Event::Return && fileSearchResultsMenu->Focused()) {
                fileAbsolutePath =
                    fs::absolute((fs::path)fileSearchResultsVector[selectedFileResultsIndex])
                        .string();
                system(("code \"" + fileAbsolutePath + "\"").c_str());
                return true;
            }
            return searchContent->OnEvent(event);
        }
        if (event == Event::Return && menu->Focused()) {
            std::string selection = currentDirectoryVector[selectedIndex];
            if (selection == BACK) {
                currentPath = currentPath.parent_path();
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
            } else if (selection == QUIT) {
                screen.ExitLoopClosure()();
            } else if (fs::is_directory(currentPath / selection)) {
                currentPath /= selection;
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
            } else {
                fileAbsolutePath = fs::absolute(currentPath / selection).string();
                isFileFocused = true;
                FileMetadata meta = getFileData(fileAbsolutePath);
                fileMetadataString =
                    fileAbsolutePath + " | Size: " + std::to_string(meta.file_size);
                fileActions->TakeFocus();
            }
            return true;
        }
        if (event == Event::Escape) {
            isFileFocused = false;
            menu->TakeFocus();
            return true;
        }
        if (event == Event::ArrowRight && menu->Focused()) {
            if (isFileFocused)
                fileActions->TakeFocus();
            else
                dirActions->TakeFocus();
            return true;
        }
        return false;
    });

    screen.Loop(main_renderer);
    return 0;
}