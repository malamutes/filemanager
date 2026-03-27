#include "constants/constants.hpp"
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "include/data_fetching/data_fetching.hpp"
#include "include/utils/utils.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <inttypes.h>
#include <iostream>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;
namespace fs = std::filesystem;
std::mutex refreshSystemData_mutex;

int main() {
    auto screen = ScreenInteractive::TerminalOutput();
    std::vector<ProcessIDMetadata> userProcessIDMetadata;
    std::vector<std::string> process_table_list;
    int process_selected = 0;

    std::thread refreshThread(refreshProcessData, std::ref(userProcessIDMetadata), &screen,
                              std::ref(refreshSystemData_mutex));
    refreshThread.detach();

    fs::path currentPath = fs::current_path();
    std::vector<std::string> currentDirectoryVector;
    std::vector<std::string> fileSearchResultsVector;
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
        if (label != Filemanager::Constants::QUIT && label != Filemanager::Constants::BACK) {
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
    auto process_table_menu = Menu(&process_table_list, &process_selected);

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

    auto searchRenderer =
        Renderer(Container::Vertical({fileSearchInput, fileSearchResultsMenu}), [&] {
            return vbox({text("Search in: " + currentPath.string()) | dim,
                         fileSearchInput->Render() | border, separator(),
                         fileSearchResultsMenu->Render() | vscroll_indicator | frame | flex});
        });

    auto systemInformationContent = Container::Vertical(
        {Renderer([&] {
             refeshBatteryInfo(currentBattery);
             refreshCPUInfo(currentCPU);
             getStorageInformation(currentStorage);
             {
                 std::lock_guard<std::mutex> guard(refreshSystemData_mutex);
                 process_table_list.clear();

                 // Calculate column widths
                 size_t pid_width = 5;
                 size_t name_width = 12;
                 size_t mem_width = 8;

                 for (auto const &p : userProcessIDMetadata) {
                     pid_width = std::max(pid_width, p.pid.length());
                     name_width = std::max(name_width, p.process_name.length());
                     mem_width = std::max(mem_width, (p.vmrss.empty() ? 6 : p.vmrss.length()));
                 }

                 // Add header
                 std::string header = "PID";
                 header.resize(pid_width, ' ');
                 header += "  Name";
                 header.resize(pid_width + 2 + name_width, ' ');
                 header += "  Memory";
                 header.resize(pid_width + 2 + name_width + 2 + mem_width, ' ');
                 header += "  State";
                 process_table_list.push_back(header);

                 // Add separator
                 process_table_list.push_back(std::string(header.length(), '-'));

                 // Add rows
                 for (auto const &p : userProcessIDMetadata) {
                     std::string row = p.pid;
                     row.resize(pid_width, ' ');
                     row += "  " + p.process_name;
                     row.resize(pid_width + 2 + name_width, ' ');
                     std::string mem = p.vmrss.empty() ? "Kernel" : p.vmrss;
                     row += "  " + mem;
                     row.resize(pid_width + 2 + name_width + 2 + mem_width, ' ');
                     row += "  " + p.state;
                     process_table_list.push_back(row);
                 }
             }
             float bat_ratio =
                 std::stof(currentBattery.battery.empty() ? "0" : currentBattery.battery) / 100.0f;
             float storage_ratio =
                 (currentStorage.capacity == 0)
                     ? 0.0f
                     : (float)(currentStorage.capacity - currentStorage.available) /
                           (float)currentStorage.capacity;
             return vbox({
                 hbox({
                     vbox({
                         text("  SYSTEM ") | bold | color(Color::Blue),
                         hbox({text("  CPU Model:  "), text(currentCPU.model_name) | dim}),
                         hbox({text("  Topology:   "), text(currentCPU.cores + " Cores / " +
                                                            currentCPU.threads + " Threads")}),
                         hbox({text("  Clock:      "),
                               text(currentCPU.current_mhz + " MHz") | color(Color::Yellow)}),
                     }) | flex,
                     separator(),
                     vbox({
                         text("  SESSION ") | bold | color(Color::Blue),
                         hbox({text("  Path:       "),
                               text(currentPath.filename().string()) | color(Color::Yellow)}),
                     }) | size(WIDTH, EQUAL, 30),
                 }),

                 separatorDouble(),

                 // 2. MIDDLE SECTION: Storage and Power Gauges
                 hbox({
                     vbox({
                         text(" STORAGE USAGE ") | bold | color(Color::Magenta),
                         hbox({gauge(storage_ratio) | color(Color::Magenta) | border,
                               vbox({text(" " +
                                          std::to_string(
                                              (currentStorage.capacity - currentStorage.available) /
                                              Filemanager::Constants::GIBIBYTES_CONVERSION) +
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

                 separatorDashed(),
             });
         }),
         Renderer(process_table_menu, [&] {
             return process_table_menu->Render() | vscroll_indicator | frame | border | flex;
         })});

    auto systemInformationRenderer = Renderer(systemInformationContent, [&] {
        return vbox({text("SYSTEM INFORMATION") | bold | center, separator(),
                     systemInformationContent->Render()}) |
               border;
    });

    int main_tab_selected = 0;
    std::vector<std::string> tab_titles = {"Browse Files", "Search Files", "System Information"};
    auto main_tabs = Container::Tab({browseRenderer, searchRenderer, systemInformationRenderer},
                                    &main_tab_selected);

    auto main_renderer = Renderer(main_tabs, [&] {
        Elements tab_elements;
        for (size_t i = 0; i < tab_titles.size(); i++) {
            auto e = text(tab_titles[i]);
            if (i == (size_t)main_tab_selected) {
                e = e | bold | underlined;
            }
            tab_elements.push_back(e);
            if (i < tab_titles.size() - 1) {
                tab_elements.push_back(text(" | "));
            }
        }

        auto base_ui = vbox({hbox(tab_elements) | center, separator(), main_tabs->Render() | flex,
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
        if (event == Event::Tab) {
            main_tab_selected = (main_tab_selected + 1) % tab_titles.size();
            return true;
        };

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
                try {
                    for (auto const &dir_entry : fs::recursive_directory_iterator(currentPath)) {
                        if (dir_entry.path().filename().string().find(searchQuery) !=
                            std::string::npos)
                            fileSearchResultsVector.push_back(dir_entry.path().string());
                    }
                } catch (...) {
                }
                return true;
            } else if (event == Event::Return && fileSearchResultsMenu->Focused()) {
                if (!fileSearchResultsVector.empty()) {
                    fileAbsolutePath =
                        fs::absolute((fs::path)fileSearchResultsVector[selectedFileResultsIndex])
                            .string();
                    system(("code \"" + fileAbsolutePath + "\"").c_str());
                }
                return true;
            }
            return searchRenderer->OnEvent(event);
        }
        if (event == Event::Return && menu->Focused()) {
            std::string selection = currentDirectoryVector[selectedIndex];
            if (selection == Filemanager::Constants::BACK) {
                currentPath = currentPath.parent_path();
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
            } else if (selection == Filemanager::Constants::QUIT) {
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