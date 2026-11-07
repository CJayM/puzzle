cmake_minimum_required(VERSION 3.25)
project(JigsawPuzzle VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Generate version header ---
configure_file(
    ${CMAKE_SOURCE_DIR}/src/core/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/Version.h
    @ONLY
)

# --- Generate Windows version resource ---
if(WIN32)
    configure_file(
        ${CMAKE_SOURCE_DIR}/src/version.rc.in
        ${CMAKE_BINARY_DIR}/generated/version.rc
        @ONLY
    )
endif()

# --- raylib (local install at C:/raylib) ---
# Enable JPEG support (disabled by default in raylib config.h)
add_compile_definitions(SUPPORT_FILEFORMAT_JPG=1)
add_subdirectory(C:/raylib/raylib ${CMAKE_BINARY_DIR}/raylib)

# --- Main executable ---
add_executable(JigsawPuzzle)

target_sources(JigsawPuzzle PRIVATE
    src/main.cpp
    src/core/Config.cpp
    src/core/Game.cpp
    src/core/ResourceManager.cpp
    src/core/SaveManager.cpp
    src/states/MainMenuState.cpp
    src/states/MenuState.cpp
    src/states/DifficultyState.cpp
    src/states/PlayState.cpp
    src/states/CompletedState.cpp
    src/states/SettingsState.cpp
    src/ui/Button.cpp
    src/ui/Label.cpp
    src/puzzle/PuzzleGenerator.cpp
    src/puzzle/PieceShapeGenerator.cpp
    src/puzzle/SnapSystem.cpp
    src/camera/CameraController.cpp
)

target_include_directories(JigsawPuzzle PRIVATE src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(JigsawPuzzle PRIVATE raylib)

# Windows executable icon + version (compiled from src/icon.rc with windres)
if(WIN32)
    find_program(WINDRES windres)
    if(WINDRES)
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/app_icon.o
            COMMAND ${WINDRES} -i ${CMAKE_SOURCE_DIR}/src/icon.rc
                           -i ${CMAKE_BINARY_DIR}/generated/version.rc
                           -o ${CMAKE_BINARY_DIR}/app_icon.o
            DEPENDS ${CMAKE_SOURCE_DIR}/src/icon.rc
                    ${CMAKE_BINARY_DIR}/generated/version.rc
                    ${CMAKE_SOURCE_DIR}/assets/icon.ico
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        target_sources(JigsawPuzzle PRIVATE ${CMAKE_BINARY_DIR}/app_icon.o)
    endif()
endif()

# Copy assets to build dir on every build (custom target has no outputs,
# so it always re-runs — keeps assets/images in sync with new pictures)
add_custom_command(TARGET JigsawPuzzle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:JigsawPuzzle>/assets
)

add_custom_target(CopyAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:JigsawPuzzle>/assets
)
