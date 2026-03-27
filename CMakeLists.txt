cmake_minimum_required(VERSION 3.16)
project(FileManager)
set(CMAKE_CXX_STANDARD 17)
find_package(ftxui REQUIRED)
file(GLOB_RECURSE SOURCES "src/**/*.cpp")
add_executable(main 
    main.cpp 
    ${SOURCES}
)
target_link_libraries(
    main
    PRIVATE ftxui::screen
    PRIVATE ftxui::component
    PRIVATE ftxui::dom
)      