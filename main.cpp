#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <cstdlib>
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
using namespace ftxui;
namespace fs = std::filesystem;

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
        oldDirectoryVector.push_back(dir_entry.path().string());
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

    refeshDirectoryVector(currentPath, currentDirectoryVector);

    int selectedIndex = 0;
    auto menu = Menu(&currentDirectoryVector, &selectedIndex);

    int selectedFileResultsIndex = 0;
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
                fs::remove((fs::path)fileAbsolutePath);
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
                menu->TakeFocus();
            }
        },
        ButtonOption::Animated());

    auto btn_create_file = Button(
        "Create New File",
        [&] { fileMetadataString = "Creating file in: " + currentPath.string(); },
        ButtonOption::Animated());

    auto fileActions = Container::Vertical({
        btn_open_vs_file,
        btn_delete_file,
    });

    auto dirActions = Container::Vertical({
        btn_open_vs_dir,
        btn_create_file,
    });

    auto rightPanelContainer = Container::Vertical({
        fileActions,
        dirActions,
    });

    auto browseContent = Container::Horizontal({
        menu,
        rightPanelContainer,
    });

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
        return vbox({
            text("Search in: " + currentPath.string()) | dim,
            fileSearchInput->Render() | border,
            separator(),
            fileSearchResultsMenu->Render() | vscroll_indicator | frame | flex,
        });
    });

    int main_tab_selected = 0;
    std::vector<std::string> tab_titles = {"Browse Files", "Search Files"};
    auto tab_toggle = Toggle(&tab_titles, &main_tab_selected);
    auto main_tabs = Container::Tab({browseRenderer, searchRenderer}, &main_tab_selected);

    auto main_renderer = Renderer(Container::Vertical({tab_toggle, main_tabs}), [&] {
        return vbox({tab_toggle->Render() | center, separator(), main_tabs->Render() | flex,
                     separator(), text(fileMetadataString) | dim}) |
               border;
    });

    main_renderer |= CatchEvent([&](Event event) -> bool {
        if (main_tab_selected == 1) {
            if (event == Event::Return && !fileSearchResultsMenu->Focused()) {
                fileSearchResultsVector.clear();
                for (auto const &dir_entry : fs::recursive_directory_iterator(currentPath)) {
                    if (dir_entry.path().filename().string().find(searchQuery) !=
                        std::string::npos) {
                        fileSearchResultsVector.push_back(dir_entry.path().string());
                    }
                }
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
            } else if (fs::is_directory((fs::path)selection)) {
                currentPath = (fs::path)selection;
                refeshDirectoryVector(currentPath, currentDirectoryVector);
                isFileFocused = false;
            } else {
                fileAbsolutePath = fs::absolute((fs::path)selection).string();
                isFileFocused = true;
                FileMetadata meta = getFileData(fileAbsolutePath);
                fileMetadataString = fileAbsolutePath +
                                     " | Size: " + std::to_string(meta.file_size) +
                                     " | Type: " + file_type_to_string(meta.file_status.type());
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