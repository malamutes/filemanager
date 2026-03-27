#include "constants/constants.hpp"
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "include/data_fetching/data_fetching.hpp"
#include "include/ui_component/ui_component.hpp"
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
    auto screen = ScreenInteractive::Fullscreen();
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
    bool isSearchFileFocused = false;
    bool showCreateModal = false;
    bool showRenameModal = false;
    bool showSearchInFileModal = false;
    std::string newFileName = "";
    std::string renameInputStr = "";
    BatteryInfo currentBattery;
    CPUInfo currentCPU;
    fs::space_info currentStorage;
    std::string searchInFileQuery = "";
    std::vector<std::string> inFileMatchResults = {};
    std::vector<std::string> userListVector = {};

    refeshDirectoryVector(currentPath, currentDirectoryVector);
    refreshUserList(userListVector);
    std::sort(userListVector.begin(), userListVector.end());
    int selectedIndex = 0;
    int selectedFileResultsIndex = 0;
    int selectedInFileMatchIndex = 0;

    MenuOption option;
    option.entries_option.transform = [&](const EntryState &state) {
        std::string label = state.label;
        Element e = text(label);

        auto prettifySpecialLabel = [](const std::string &raw) {
            std::string pretty = raw;
            std::replace(pretty.begin(), pretty.end(), '_', ' ');
            bool capitalize_next = true;
            for (char &c : pretty) {
                if (c == ' ') {
                    capitalize_next = true;
                    continue;
                }
                if (capitalize_next) {
                    c = std::toupper(static_cast<unsigned char>(c));
                    capitalize_next = false;
                } else {
                    c = std::tolower(static_cast<unsigned char>(c));
                }
            }
            return pretty;
        };

        if (label == Filemanager::Constants::QUIT_PROGRAM) {
            e = hbox({text("⏻ ") | color(Color::RedLight), text(prettifySpecialLabel(label)) | color(Color::RedLight)});
        } else if (label == Filemanager::Constants::PARENT_DIRECTORY) {
            e = hbox({text("↩ ") | color(Color::Cyan), text(prettifySpecialLabel(label)) | color(Color::Cyan)});
        } else {
            std::string icon = determineFileIcon(currentPath / label);
            e = hbox({text(icon) | color(Color::Yellow), text(label)});
        }
        if (state.focused)
            e |= inverted;
        if (state.active)
            e |= bold;
        return e;
    };

    auto menu = UIComponents::createMenu(&currentDirectoryVector, &selectedIndex, option);
    auto fileSearchResultsMenu = UIComponents::createMenu(&fileSearchResultsVector, &selectedFileResultsIndex);
    auto inFileMatchResultsMenu = UIComponents::createMenu(&inFileMatchResults, &selectedInFileMatchIndex);
    auto fileSearchInput = Input(&searchQuery, "Type filename...");
    auto searchInFileInput = Input(&searchInFileQuery, "Type text to find...");
    auto process_table_menu = UIComponents::createTable(&process_table_list, &process_selected);
    std::function<void()> runSearchWithinSelectedFile;

    auto btn_open_vs_file =
        UIComponents::createButton("Open File in VS Code", [&] { system(("code \"" + fileAbsolutePath + "\"").c_str()); });
    auto btn_search_open_vs_file =
        UIComponents::createButton("Open File in VS Code", [&] { system(("code \"" + fileAbsolutePath + "\"").c_str()); });
    auto btn_search_query_in_file = UIComponents::createButton("Search Within File", [&] {
        showSearchInFileModal = true;
        searchInFileInput->TakeFocus();
    });
    auto btn_open_vs_dir = UIComponents::createButton("Open Dir in VS Code",
                                                      [&] { system(("code \"" + currentPath.string() + "\"").c_str()); });
    auto btn_delete_file = UIComponents::createButton("Delete File", [&] {
        if (fs::exists(fileAbsolutePath)) {
            fs::remove(fileAbsolutePath);
            refeshDirectoryVector(currentPath, currentDirectoryVector);
            isFileFocused = false;
            menu->TakeFocus();
        }
    });
    auto btn_create_file = UIComponents::createButton("Create New File", [&] { showCreateModal = true; });
    auto btn_rename_file = UIComponents::createButton("Rename File", [&] {
        renameInputStr = fs::path(fileAbsolutePath).filename().string();
        showRenameModal = true;
    });

    auto fileActions = Container::Vertical({btn_open_vs_file, btn_rename_file, btn_delete_file});
    auto dirActions = Container::Vertical({btn_open_vs_dir, btn_create_file});
    auto fileNameInput = Input(&newFileName, "filename.txt");
    auto fileNameSubmit = UIComponents::createButton("Create", [&] {
        if (!newFileName.empty()) {
            std::ofstream(currentPath / newFileName).close();
            refeshDirectoryVector(currentPath, currentDirectoryVector);
            showCreateModal = false;
            newFileName = "";
            menu->TakeFocus();
        }
    });
    auto renameInputComp = Input(&renameInputStr, "");
    auto renameSubmit = UIComponents::createButton("Rename", [&] {
        if (!renameInputStr.empty()) {
            fs::path oldPath = fileAbsolutePath;
            fs::path newPath = oldPath.parent_path() / renameInputStr;
            fs::rename(oldPath, newPath);
            refeshDirectoryVector(currentPath, currentDirectoryVector);
            showRenameModal = false;
            isFileFocused = false;
            menu->TakeFocus();
        }
    });
    auto searchInFileSubmitButton = UIComponents::createButton("Search", [&] {
        if (runSearchWithinSelectedFile) {
            runSearchWithinSelectedFile();
        }
    });

    auto create_modal_container = Container::Vertical({fileNameInput, fileNameSubmit});
    auto rename_modal_container = Container::Vertical({renameInputComp, renameSubmit});
    auto searchInFileModalContainer = Container::Vertical({searchInFileInput, searchInFileSubmitButton});
    auto browseContent = Container::Horizontal({menu, Container::Vertical({fileActions, dirActions})});
    auto searchFileActions = Container::Vertical({btn_search_open_vs_file, btn_search_query_in_file});
    auto searchContent = Container::Horizontal(
        {Container::Vertical({fileSearchInput, fileSearchResultsMenu, inFileMatchResultsMenu}), searchFileActions});

    runSearchWithinSelectedFile = [&] {
        inFileMatchResults.clear();
        selectedInFileMatchIndex = 0;

        if (fileAbsolutePath.empty()) {
            fileMetadataString = "Select a file in Search Files before searching within it.";
            showSearchInFileModal = false;
            searchFileActions->TakeFocus();
            return;
        }

        fs::path targetPath = fs::path(fileAbsolutePath);
        if (!fs::exists(targetPath) || fs::is_directory(targetPath)) {
            fileMetadataString = "Selected path is not a searchable file: " + fileAbsolutePath;
            showSearchInFileModal = false;
            searchFileActions->TakeFocus();
            return;
        }

        if (searchInFileQuery.empty()) {
            fileMetadataString = "Enter text to search within file: " + fileAbsolutePath;
            showSearchInFileModal = false;
            inFileMatchResultsMenu->TakeFocus();
            return;
        }

        searchQueryInFile(searchInFileQuery, inFileMatchResults, fs::absolute(targetPath));
        showSearchInFileModal = false;
        fileMetadataString = "Matches in " + fileAbsolutePath + ": " + std::to_string(inFileMatchResults.size());
        inFileMatchResultsMenu->TakeFocus();
    };

    auto browseRenderer = Renderer(browseContent, [&] {
        auto title = isFileFocused ? " [FILE ACTIONS] " : " [DIR ACTIONS] ";
        auto active_content = isFileFocused ? fileActions->Render() : dirActions->Render();
        return hbox({menu->Render() | flex, separator(),
                     vbox({text(title) | bold | center | color(Color::Yellow), separator(), active_content}) |
                         size(WIDTH, EQUAL, 25)});
    });

    auto searchRenderer = Renderer(searchContent, [&] {
        auto searchActionsContent = isSearchFileFocused
                                        ? searchFileActions->Render()
                                        : vbox({text("Select a result") | dim, text("Press Enter/Right") | dim});
        return hbox(
            {vbox({text("Search in: " + currentPath.string()) | dim, fileSearchInput->Render() | border, separator(),
                   text("Filename Matches") | bold, fileSearchResultsMenu->Render() | vscroll_indicator | frame, separator(),
                   text("In-File Matches") | bold, inFileMatchResultsMenu->Render() | vscroll_indicator | frame | flex}) |
                 flex,
             separator(),
             vbox({text(" [SEARCH ACTIONS] ") | bold | center | color(Color::Yellow), separator(), searchActionsContent}) |
                 size(WIDTH, EQUAL, 25)});
    });

    std::vector<Component> userCardComponents;
    for (const auto &username : userListVector) {
        userCardComponents.push_back(UIComponents::createUserCard(
            username,
            [&, username] { fileMetadataString = "TODO: Swap to user -> " + username; },
            [&, username] { fileMetadataString = "TODO: Delete user -> " + username; }));
    }
    auto userCardsContainer = Container::Vertical(userCardComponents);
    auto userListRenderer = Renderer(userCardsContainer, [&] {
        if (userListVector.empty()) {
            return vbox({text("USER LIST") | bold | center, separator(),
                         text("No regular users found in /etc/passwd.") | dim | center}) |
                   border;
        }

        return vbox({
                   text("USER LIST") | bold | center,
                   separator(),
                   text("Accounts loaded from /etc/passwd") | dim | center,
                   separator(),
                   userCardsContainer->Render() | vscroll_indicator | frame | flex,
               }) |
               border;
    });

    auto systemInformationContent = Container::Vertical(
        {Renderer([&] {
             refeshBatteryInfo(currentBattery);
             refreshCPUInfo(currentCPU);
             getStorageInformation(currentStorage);
             {
                 std::lock_guard<std::mutex> guard(refreshSystemData_mutex);
                 process_table_list.clear();

                 size_t pid_width = 5;
                 size_t name_width = 12;
                 size_t mem_width = 8;

                 for (auto const &p : userProcessIDMetadata) {
                     pid_width = std::max(pid_width, p.pid.length());
                     name_width = std::max(name_width, p.process_name.length());
                     mem_width = std::max(mem_width, (p.vmrss.empty() ? 6 : p.vmrss.length()));
                 }

                 std::string header = "PID";
                 header.resize(pid_width, ' ');
                 header += "  Name";
                 header.resize(pid_width + 2 + name_width, ' ');
                 header += "  Memory";
                 header.resize(pid_width + 2 + name_width + 2 + mem_width, ' ');
                 header += "  State";
                 process_table_list.push_back(header);

                 process_table_list.push_back(std::string(header.length(), '-'));

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
             float bat_ratio = std::stof(currentBattery.battery.empty() ? "0" : currentBattery.battery) / 100.0f;
             float storage_ratio =
                 (currentStorage.capacity == 0)
                     ? 0.0f
                     : (float)(currentStorage.capacity - currentStorage.available) / (float)currentStorage.capacity;
             return vbox({
                 hbox({
                     vbox({
                         text("  SYSTEM ") | bold | color(Color::Blue),
                         UIComponents::createStatBox("  CPU Model:  ", currentCPU.model_name),
                         UIComponents::createStatBox("  Topology:   ",
                                                     currentCPU.cores + " Cores / " + currentCPU.threads + " Threads"),
                         UIComponents::createStatBox("  Clock:      ", currentCPU.current_mhz + " MHz", Color::Yellow),
                     }) | flex,
                     separator(),
                     vbox({
                         text("  SESSION ") | bold | color(Color::Blue),
                         UIComponents::createStatBox("  Path:       ", currentPath.filename().string(), Color::Yellow),
                     }) | size(WIDTH, EQUAL, 30),
                 }),

                 separatorDouble(),

                 hbox({
                     vbox({
                         text(" STORAGE USAGE ") | bold | color(Color::Magenta),
                         UIComponents::createGaugeBox(
                             storage_ratio,
                             " " +
                                 std::to_string((currentStorage.capacity - currentStorage.available) /
                                                Filemanager::Constants::GIBIBYTES_CONVERSION) +
                                 " GB",
                             " " + std::to_string((int)(storage_ratio * 100)) + "% Used", Color::Magenta),
                     }) | flex,
                     separator(),
                     vbox({
                         text(" POWER STATUS ") | bold | color(Color::Cyan),
                         UIComponents::createGaugeBox(bat_ratio, " " + currentBattery.battery + "%",
                                                      " " + currentBattery.status,
                                                      currentBattery.status == "Charging" ? Color::Green : Color::Cyan),
                     }) | flex,
                 }),

                 separatorDashed(),
             });
         }),
         process_table_menu});

    auto systemInformationRenderer = Renderer(systemInformationContent, [&] {
        return vbox({text("SYSTEM INFORMATION") | bold | center, separator(), systemInformationContent->Render()}) | border;
    });

    int main_tab_selected = 0;
    std::vector<std::string> tab_titles = {"Browse Files", "Search Files", "System Information", "User List"};
    auto main_tabs =
        Container::Tab({browseRenderer, searchRenderer, systemInformationRenderer, userListRenderer}, &main_tab_selected);

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

        auto base_ui = vbox({hbox(tab_elements) | center, separator(), main_tabs->Render() | flex, separator(),
                             text(fileMetadataString) | dim}) |
                       border;
        if (showCreateModal) {
            return dbox({base_ui | dim, vbox({text("Enter New File Name:") | bold | center, fileNameInput->Render() | border,
                                              fileNameSubmit->Render() | center}) |
                                            border | center | size(WIDTH, GREATER_THAN, 40) | bgcolor(Color::Black)});
        }
        if (showRenameModal) {
            return dbox({base_ui | dim, vbox({text("Rename File To:") | bold | center, renameInputComp->Render() | border,
                                              renameSubmit->Render() | center}) |
                                            border | center | size(WIDTH, GREATER_THAN, 40) | bgcolor(Color::Black)});
        }
        if (showSearchInFileModal) {
            return dbox(
                {base_ui | dim, vbox({text("Search Text Within Selected File:") | bold | center,
                                      searchInFileInput->Render() | border, searchInFileSubmitButton->Render() | center}) |
                                    border | center | size(WIDTH, GREATER_THAN, 50) | bgcolor(Color::Black)});
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
        if (showSearchInFileModal) {
            if (event == Event::Escape) {
                showSearchInFileModal = false;
                searchFileActions->TakeFocus();
                return true;
            }
            if (event == Event::Return) {
                if (runSearchWithinSelectedFile) {
                    runSearchWithinSelectedFile();
                }
                return true;
            }
            return searchInFileModalContainer->OnEvent(event);
        }
        if (main_tab_selected == 1) {
            auto focusSearchSelection = [&]() {
                if (fileSearchResultsVector.empty()) {
                    return true;
                }
                fileAbsolutePath = fs::absolute((fs::path)fileSearchResultsVector[selectedFileResultsIndex]).string();
                isSearchFileFocused = true;
                FileMetadata meta = getFileData(fileAbsolutePath);
                fileMetadataString = fileAbsolutePath + " | Size: " + std::to_string(meta.file_size);
                searchFileActions->TakeFocus();
                return true;
            };

            if (event == Event::Return && fileSearchInput->Focused()) {
                fileSearchResultsVector.clear();
                selectedFileResultsIndex = 0;
                inFileMatchResults.clear();
                selectedInFileMatchIndex = 0;
                isSearchFileFocused = false;
                try {
                    for (auto const &dir_entry : fs::recursive_directory_iterator(currentPath)) {
                        if (dir_entry.path().filename().string().find(searchQuery) != std::string::npos)
                            fileSearchResultsVector.push_back(dir_entry.path().string());
                    }
                } catch (...) {
                }
                return true;
            }
            if (event == Event::Return && fileSearchResultsMenu->Focused()) {
                return focusSearchSelection();
            }
            if (event == Event::ArrowRight && fileSearchResultsMenu->Focused()) {
                return focusSearchSelection();
            }
            if (event == Event::ArrowLeft && searchFileActions->Focused()) {
                isSearchFileFocused = false;
                fileSearchResultsMenu->TakeFocus();
                return true;
            }
            if (event == Event::Escape && isSearchFileFocused) {
                isSearchFileFocused = false;
                fileSearchResultsMenu->TakeFocus();
                return true;
            }
            return searchRenderer->OnEvent(event);
        }
        if (main_tab_selected == 0 && event == Event::Return && menu->Focused()) {
            std::string selection = currentDirectoryVector[selectedIndex];
            if (selection == Filemanager::Constants::PARENT_DIRECTORY) {
                currentPath = currentPath.parent_path();
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
            } else if (selection == Filemanager::Constants::QUIT_PROGRAM) {
                screen.ExitLoopClosure()();
            } else if (fs::is_directory(currentPath / selection)) {
                currentPath /= selection;
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
            } else {
                fileAbsolutePath = fs::absolute(currentPath / selection).string();
                isFileFocused = true;
                FileMetadata meta = getFileData(fileAbsolutePath);
                fileMetadataString = fileAbsolutePath + " | Size: " + std::to_string(meta.file_size);
                fileActions->TakeFocus();
            }
            return true;
        }
        if (main_tab_selected == 0 && event == Event::Escape) {
            isFileFocused = false;
            menu->TakeFocus();
            return true;
        }
        if (main_tab_selected == 0 && event == Event::ArrowRight && menu->Focused()) {
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
