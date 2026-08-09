cmake_minimum_required(VERSION 3.22)
project(JigsawPuzzle LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Force PLATFORM=Android BEFORE including raylib's CMake
# ============================================================
set(PLATFORM Android CACHE STRING "Platform to build for" FORCE)

# ============================================================
# Build raylib as STATIC library using its own CMake
# (raylib's CMake handles Android: PLATFORM_ANDROID, GLES2,
#  native_app_glue from NDK, EGL/GLESv2/OpenSLES links,
#  -Wl,--wrap=fopen for APK asset reads)
# ============================================================
set(RAYLIB_ROOT "C:/raylib/raylib")

# Enable JPEG support in raylib (disabled by default in config.h)
add_compile_definitions(SUPPORT_FILEFORMAT_JPG=1)

add_subdirectory("${RAYLIB_ROOT}" "${CMAKE_BINARY_DIR}/raylib" EXCLUDE_FROM_ALL)

# ============================================================
# Path to the project source tree:
# android/app/src/main/cpp -> project root is 5 levels up
# ============================================================
set(PUZZLE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
get_filename_component(PUZZLE_ROOT "${PUZZLE_ROOT}" ABSOLUTE)

# ============================================================
# Build our game as SHARED library
# ============================================================

add_library(jigsaw SHARED
    main.cpp
    AndroidAssets.cpp
    ${PUZZLE_ROOT}/src/core/Config.cpp
    ${PUZZLE_ROOT}/src/core/Game.cpp
    ${PUZZLE_ROOT}/src/core/ResourceManager.cpp
    ${PUZZLE_ROOT}/src/core/SaveManager.cpp
    ${PUZZLE_ROOT}/src/states/MainMenuState.cpp
    ${PUZZLE_ROOT}/src/states/MenuState.cpp
    ${PUZZLE_ROOT}/src/states/DifficultyState.cpp
    ${PUZZLE_ROOT}/src/states/PlayState.cpp
    ${PUZZLE_ROOT}/src/states/CompletedState.cpp
    ${PUZZLE_ROOT}/src/states/SettingsState.cpp
    ${PUZZLE_ROOT}/src/ui/Button.cpp
    ${PUZZLE_ROOT}/src/puzzle/PuzzleGenerator.cpp
    ${PUZZLE_ROOT}/src/puzzle/PieceShapeGenerator.cpp
    ${PUZZLE_ROOT}/src/puzzle/SnapSystem.cpp
    ${PUZZLE_ROOT}/src/camera/CameraController.cpp
)

target_compile_definitions(jigsaw PRIVATE
    PLATFORM_ANDROID
    GRAPHICS_API_OPENGL_ES2
)

target_include_directories(jigsaw PRIVATE
    ${PUZZLE_ROOT}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${ANDROID_NDK}/sources/android/native_app_glue"
)

target_link_libraries(jigsaw PRIVATE
    raylib
    log
    android
    EGL
    GLESv2
    OpenSLES
    atomic
    c
    m
)

# Pull in android_native_app_glue.o from libraylib.a and export its entry
# point; without this the loader reports "undefined symbol: ANativeActivity_onCreate"
target_link_options(jigsaw PRIVATE "-Wl,-u,ANativeActivity_onCreate")
