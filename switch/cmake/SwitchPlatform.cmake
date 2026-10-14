# Platform setup for the Nintendo Switch build. Included from the top-level CMakeLists.txt, right after
# project(), so that everything below it (including the bundled dependencies in extern/) sees these settings.

# devkitA64's newlib only declares POSIX functions when GNU extensions are enabled.
set(CMAKE_CXX_EXTENSIONS ON)

# CMake's precompiled header support writes absolute paths into a generated header. On Windows the build runs
# in MSYS2, whose /c/... paths the (native) compiler only understands on its command line, not inside files.
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

# Headers for the devkitPro portlibs (EGL, GL, SDL2, ...), for the compatibility layer (dlfcn.h), for
# the switch that puts Mesa's work on a thread of its own (nxglthread.h) and for writing out the counts
# of a build made for profile-guided optimisation (nxpgo.h).
find_path(OPENMW_SWITCH_PORTLIBS_INCLUDE_DIR NAMES EGL/egl.h REQUIRED)
include_directories(SYSTEM
    "${CMAKE_CURRENT_LIST_DIR}/../compat/include"
    "${CMAKE_CURRENT_LIST_DIR}/../glthread"
    "${CMAKE_CURRENT_LIST_DIR}/../pgo"
    "${OPENMW_SWITCH_PORTLIBS_INCLUDE_DIR}"
)

# Profile-guided optimisation, see ../README.md. switch/build.sh sets this with --pgo.
set(OPENMW_SWITCH_PGO "OFF" CACHE STRING
    "Profile-guided optimisation: OFF, GENERATE (count what is executed) or USE (optimise by the counts)")
set_property(CACHE OPENMW_SWITCH_PGO PROPERTY STRINGS OFF GENERATE USE)
if(NOT OPENMW_SWITCH_PGO MATCHES "^(OFF|GENERATE|USE)$")
    message(FATAL_ERROR "OPENMW_SWITCH_PGO is '${OPENMW_SWITCH_PGO}': expected OFF, GENERATE or USE")
endif()

# Mesa on Switch has no libGL, switch/glshim provides the entry points instead. Presetting the result
# variable makes find_package(OpenGL) succeed with it, both here and in OpenSceneGraph's build.
set(OPENGL_gl_LIBRARY nxgl CACHE STRING "OpenGL entry points for Nintendo Switch" FORCE)

# OpenAL Soft is a static library here.
add_compile_definitions(AL_LIBTYPE_STATIC)

# devkitPro's SDL2 package only defines the static target.
find_package(SDL2 REQUIRED CONFIG)
if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES INTERFACE_LINK_LIBRARIES SDL2::SDL2-static)
endif()

# Libraries devkitPro does not package: the compatibility layer, the OpenGL shim, Boost and ICU, and the
# addition to its Mesa.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_BINARY_DIR}/switch")

# Only from here on, so that the libraries above are compiled the same way in every build: none of them
# is where the time goes, and the memory allocator must not be counted, because counting allocates.
if(OPENMW_SWITCH_PGO STREQUAL "GENERATE")
    add_compile_options(
        -fprofile-generate
        # Several threads run the same code.
        -fprofile-update=atomic
        # Hand the counters to nxpgo.c instead of to the code that would write them to the build
        # directory, which is on another computer.
        -fprofile-info-section=gcov_info
    )
elseif(OPENMW_SWITCH_PGO STREQUAL "USE")
    add_compile_options(
        -fprofile-use
        # Counts taken from several threads never add up exactly.
        -fprofile-correction
        # Without this, whatever did not run while the counts were taken is optimised for size. No
        # session covers all of a game, so compile that part as if there were no counts.
        -fprofile-partial-training
        # Sources that are not part of the executable have no counts.
        -Wno-missing-profile
        # A function changed since the counts were taken is compiled without them. Worth a warning,
        # but not the error it is by default.
        -Wno-error=coverage-mismatch
    )
endif()

# The executable is linked from several dozen static archives with circular dependencies between some
# of them (freetype and harfbuzz, for one). Let the linker resolve the order.
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")
