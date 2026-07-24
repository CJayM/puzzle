cmake_minimum_required(VERSION 3.25)
project(JigsawPuzzle VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- raylib (local install at C:/raylib) ---
add_subdirectory(C:/raylib/raylib ${CMAKE_BINARY_DIR}/raylib)

# --- Main executable ---
add_executable(JigsawPuzzle)

target_sources(JigsawPuzzle PRIVATE
    src/main.cpp
    src/core/Config.cpp
    src/core/Game.cpp
    src/core/ResourceManager.cpp
    src/states/MenuState.cpp
)

target_include_directories(JigsawPuzzle PRIVATE src)
target_link_libraries(JigsawPuzzle PRIVATE raylib)

# Copy assets to build dir
add_custom_command(TARGET JigsawPuzzle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:JigsawPuzzle>/assets
)
