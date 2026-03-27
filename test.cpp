#include <chrono>
#include <cstdio>
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

std::string COMMAND_TO_RUN = "git status";

int main() {

    std::ifstream userStream("/etc/passwd");
    int status;
    char buffer[1024];
    std::string result;

    FILE *returnOutputPipe = popen(COMMAND_TO_RUN.c_str(), "r");

    if (!returnOutputPipe) {
        // do something
    }

    while (fgets(buffer, sizeof(buffer), returnOutputPipe) != nullptr) {
        result += buffer;
    }

    status = pclose(returnOutputPipe);
    if (status == -1) {
        /* Error reported by pclose() */

    } else {
        /* Use macros described under wait() to inspect `status' in order
           to determine success/failure of command executed by popen() */
    }

    std::stringstream returnOutput(result);
    std::string currLine;

    while (std::getline(returnOutput, currLine)) {
        std::cout << "Line: " << currLine << std::endl;
    }

    return 0;
}