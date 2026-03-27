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

int main() {

    std::ifstream userStream("/etc/passwd");
    std::string currLine = "";

    while (std::getline(userStream, currLine)) {
        int varX = currLine.find(":");
        int startIDColon = currLine.find(":", varX + 1);
        int endIDColon = currLine.find(":", startIDColon + 1);
        int uid = std::stoi(currLine.substr(startIDColon + 1, endIDColon - startIDColon));

        if (uid >= 1000 && uid < 65534) {
            std::cout << "UID: " << uid << "-->" << currLine.substr(0, varX) << std::endl;
        }
    };

    return 0;
}