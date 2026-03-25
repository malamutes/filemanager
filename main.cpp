// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include <filesystem>
#include <functional>
#include <inttypes.h>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include <cstdlib>

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
    std::cout << currentPath.string() << std::endl;
    std::vector<std::string> currentDirectoryVector = {};
    std::string fileMetadataString = "";
    std::string fileAbsolutePath = "";

    bool rightPanelEnabled = false;

    std::vector<std::string> rightPanelOptions = {"Open in vscode", "TESTING HARHAR", "HEHEXD"};
    // will always have these values to go back
    currentDirectoryVector.push_back(QUIT);
    currentDirectoryVector.push_back(BACK);

    for (auto const &dir_entry : std::filesystem::directory_iterator{currentPath}) {
        currentDirectoryVector.push_back(dir_entry.path().string());
    }

    int selectedIndex = 0;
    int prevSelectedIndex = 0;
    auto menu = Menu(&currentDirectoryVector, &selectedIndex);

    int rightPanelOptionSelected_1 = 0;
    int tab_selected = 0;
    auto rightPanelContainer = Container::Tab(
        {
            Radiobox(&rightPanelOptions, &rightPanelOptionSelected_1),
        },
        &tab_selected);

    auto container = Container::Horizontal({
        menu,
        rightPanelContainer,
    });

    auto renderer = Renderer(container, [&] {
        if (rightPanelEnabled) {
            return vbox({
                       hbox({menu->Render(), separator(), rightPanelContainer->Render()}),
                       hbox({text(fileMetadataString)}) | border,
                   }) |
                   border;
        } else {
            return vbox({
                       hbox({menu->Render()}),
                       hbox({text(fileMetadataString)}) | border,
                   }) |
                   border;
        }
    });

    renderer |= CatchEvent([&](Event event) -> bool {
        if (event == Event::Return) {

            bool menu_focused = menu->Focused();
            bool rightpanel_focused = rightPanelContainer->Focused();

            if (menu_focused) {

                rightPanelEnabled = false;
                if (currentDirectoryVector[selectedIndex] == BACK) {
                    currentPath = currentPath.parent_path();
                    refeshDirectoryVector(currentPath, currentDirectoryVector);
                    fileMetadataString = "Visiting directory at: " + currentPath.string();

                } else if (currentDirectoryVector[selectedIndex] == QUIT) {
                    screen.ExitLoopClosure()();
                    screen.Exit();
                } else if (fs::is_directory((fs::path)currentDirectoryVector[selectedIndex])) {
                    currentPath = (fs::path)currentDirectoryVector[selectedIndex];
                    refeshDirectoryVector(currentPath, currentDirectoryVector);
                    fileMetadataString = "Visiting directory at: " + currentPath.string();
                } else { // regular file
                    fileAbsolutePath =
                        fs::absolute((fs::path)currentDirectoryVector[selectedIndex]).string();
                    FileMetadata fileMetadata = getFileData(fileAbsolutePath);
                    char str[64];
                    sprintf(str, "Value is %" PRIuMAX "\n", fileMetadata.file_size);
                    fileMetadataString =
                        fileAbsolutePath + " ==> File Size: " + (std::string)str + " || " +
                        "File Type: " + file_type_to_string(fileMetadata.file_status.type());

                    rightPanelEnabled = true;
                    // focus on right panel if regular file
                    rightPanelContainer->TakeFocus();
                }
            } else if (rightpanel_focused) {
                if (rightPanelOptionSelected_1 == 0) {
                    std::string command =
                        std::string("code ") + fs::absolute(fileAbsolutePath).string();
                    system(command.c_str());
                }
            }

            return true;
        }
        return false;
    });

    screen.Loop(renderer);

    return 0;
}