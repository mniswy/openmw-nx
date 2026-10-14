# Toolchain file for building OpenMW for Nintendo Switch homebrew.
# Wraps devkitPro's toolchain and adjusts the one default that does not suit this project.

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO is not set. Configure from a devkitPro shell (see switch/README.md).")
endif()

include("$ENV{DEVKITPRO}/cmake/Switch.cmake")

# Let CMake scan for header dependencies itself instead of using the compiler's dependency files. On Windows
# the compiler is a native program that writes C:/... paths, which the MSYS2 make that drives the build
# cannot parse ("multiple target patterns"), breaking every build after the first.
set(CMAKE_DEPENDS_USE_COMPILER FALSE)

# devkitPro prefers CMake package config files over find modules. OpenSceneGraph and MyGUI are written
# against the variables set by CMake's own find modules (FREETYPE_LIBRARIES and friends), so restore
# CMake's default order. This has to live in the toolchain file because it is re-read for every project().
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG FALSE)
