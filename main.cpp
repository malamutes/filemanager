// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"

const std::string QUIT = "QUIT";
const std::string BACK = "BACK";
using namespace ftxui;
namespace fs = std::filesystem;

std::vector<std::string> refeshDirectoryVector(fs::path newCurrentPath,
                                               std::vector<std::string> &oldDirectoryVector) {
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

    // will always have these values to go back
    currentDirectoryVector.push_back(QUIT);
    currentDirectoryVector.push_back(BACK);

    for (auto const &dir_entry : std::filesystem::directory_iterator{currentPath}) {
        currentDirectoryVector.push_back(dir_entry.path().string());
    }

    int selectedIndex = 0;
    int prevSelectedIndex = 0;
    auto menu = Menu(&currentDirectoryVector, &selectedIndex);

    auto renderer = Renderer(menu, [&] { return menu->Render(); });

    renderer |= CatchEvent([&](Event event) -> bool {
        if (event == Event::Return) {
            if (currentDirectoryVector[selectedIndex] == BACK) {
                currentPath = currentPath.parent_path();
                refeshDirectoryVector(currentPath, currentDirectoryVector);

            } else if (currentDirectoryVector[selectedIndex] == QUIT) {
                screen.ExitLoopClosure()();
                screen.Exit();
            } else {
                // if directory then go down 1 level
                if (fs::is_directory((fs::path)currentDirectoryVector[selectedIndex])) {
                }
            }
            return true;
        }
        return false;
    });
    screen.Loop(renderer);

    return 0;
}