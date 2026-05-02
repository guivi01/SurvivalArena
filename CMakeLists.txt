cmake_minimum_required(VERSION 3.16)

project(SurvivalArena LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(SurvivalArena
    src/main.cpp
)

add_custom_command(TARGET SurvivalArena POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:SurvivalArena>/assets"
)

target_link_libraries(SurvivalArena
    PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
)

if (MSVC)
    target_compile_options(SurvivalArena PRIVATE /W4)
else()
    target_compile_options(SurvivalArena PRIVATE -Wall -Wextra -Wpedantic)
endif()
