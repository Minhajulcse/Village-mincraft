# ============================================================================
#  Rural Market - a voxel ("Minecraft-like") rural village market scene
#  rendered with the OpenGL 1.x fixed-function pipeline + GLUT (freeglut).
#
#  Build commands
#  --------------
#    Configure + build (out-of-source, Release):
#        cmake -B build -DCMAKE_BUILD_TYPE=Release
#        cmake --build build -j
#        ./build/rural_market
#
#    Debug build:
#        cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
#        cmake --build build-debug -j
#
#    Clean:
#        rm -rf build
#
#  Dependencies (Debian / Ubuntu):
#        sudo apt install build-essential cmake freeglut3-dev libglu1-mesa-dev
#
#  Dependencies (MSYS2 / MinGW-w64):
#        pacman -S mingw-w64-x86_64-freeglut mingw-w64-x86_64-cmake
#
#  A plain GNU Makefile is also provided as a no-CMake fallback.
#
#  Layout: sources in src/, reference artwork in reference/, sound effects in
#  assets/ (mirrored next to the executable at build time).
# ============================================================================

cmake_minimum_required(VERSION 3.10)
project(rural_market LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Default to a Release build when the user did not pick a configuration.
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the build type: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()
message(STATUS "rural_market: build type = ${CMAKE_BUILD_TYPE}")

# ---------------------------------------------------------------------------
# OpenGL.  This project is *strictly* fixed-pipeline (glBegin/glEnd, GLU
# quadrics, gluLookAt, gluPerspective).  Force the LEGACY preference BEFORE
# find_package(OpenGL) so that on modern Linux we do not end up with a
# GLVND-only import target that leaves GLU unresolved at link time.
# ---------------------------------------------------------------------------
set(OpenGL_GL_PREFERENCE "LEGACY")

find_package(OpenGL REQUIRED)
find_package(GLUT   REQUIRED)

# ---------------------------------------------------------------------------
# GLU has to be located by hand: FindOpenGL does not reliably populate
# OPENGL_glu_LIBRARY on every platform / CMake version.
# ---------------------------------------------------------------------------
find_library(GLU_LIBRARY
    NAMES GLU glu32 MagickGLU
    HINTS
        ${OPENGL_LIBRARY_DIR}
        ENV GLU_ROOT
        ENV MINGW_PREFIX
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib64
        /usr/local/lib
        /opt/local/lib
        /mingw64/lib
    PATH_SUFFIXES lib lib64
)

if(NOT GLU_LIBRARY)
    if(OPENGL_glu_LIBRARY)
        # FindOpenGL did find it after all - use that.
        set(GLU_LIBRARY ${OPENGL_glu_LIBRARY})
        message(STATUS "rural_market: GLU taken from OPENGL_glu_LIBRARY")
    elseif(WIN32)
        # MinGW / MSVC ship glu32 with the platform SDK; plain -lglu32 works
        # even when find_library cannot see an import library.
        set(GLU_LIBRARY glu32)
        message(STATUS "rural_market: GLU not found by search, "
                       "falling back to -lglu32 (Windows/MinGW)")
    else()
        message(FATAL_ERROR
            "GLU library not found.\n"
            "  Debian/Ubuntu : sudo apt install libglu1-mesa-dev\n"
            "  Fedora        : sudo dnf install mesa-libGLU-devel\n"
            "  Arch          : sudo pacman -S glu\n"
            "Or pass -DGLU_LIBRARY=/path/to/libGLU.so to cmake.")
    endif()
else()
    message(STATUS "rural_market: GLU = ${GLU_LIBRARY}")
endif()

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_executable(rural_market
    src/main.cpp
    src/Scene.cpp
    src/Stall.cpp
    src/Customer.cpp
    src/Carriage.cpp
    src/GraphicsHelpers.cpp
)

target_include_directories(rural_market PRIVATE
    ${OPENGL_INCLUDE_DIR}
    ${GLUT_INCLUDE_DIR}
)

target_link_libraries(rural_market PRIVATE
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    ${GLU_LIBRARY}
)

if(UNIX)
    target_link_libraries(rural_market PRIVATE m)
endif()

if(WIN32)
    # winmm is REQUIRED: the ambient-audio path calls PlaySound() from the
    # Windows Multimedia API.  opengl32/glu32 are named explicitly so the
    # link succeeds even if the find_package variables came back empty.
    target_link_libraries(rural_market PRIVATE winmm opengl32 glu32)
endif()

# ---------------------------------------------------------------------------
# Warnings (never fatal - no -Werror).
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rural_market PRIVATE -Wall -Wextra)
    if(APPLE)
        # GLUT on macOS is deprecated wholesale; keep the log readable.
        target_compile_options(rural_market PRIVATE -Wno-deprecated-declarations)
    endif()
endif()

# ---------------------------------------------------------------------------
# Runtime audio: mirror assets/ next to the executable, because the program
# looks for "assets/<clip>.wav" relative to its working directory.  The plain
# make build runs from the repo root and finds assets/ as-is; this copy is what
# makes ./build/rural_market work too.  configure_file() re-runs CMake when a
# clip changes, so an edited WAV is picked up on the next build.
#
# Missing or empty assets/ is not an error - the scene just runs quieter.
# ---------------------------------------------------------------------------
file(GLOB RM_AUDIO_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.wav)

if(RM_AUDIO_ASSETS)
    foreach(clip ${RM_AUDIO_ASSETS})
        get_filename_component(clip_name ${clip} NAME)
        configure_file(${clip} ${CMAKE_CURRENT_BINARY_DIR}/assets/${clip_name} COPYONLY)
    endforeach()

    list(LENGTH RM_AUDIO_ASSETS RM_AUDIO_COUNT)
    message(STATUS "rural_market: ${RM_AUDIO_COUNT} audio clip(s) copied to "
                   "${CMAKE_CURRENT_BINARY_DIR}/assets")
else()
    message(STATUS "rural_market: no WAVs in assets/ - the scene will run silently")
endif()
