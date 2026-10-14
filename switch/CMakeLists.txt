# Libraries for the Nintendo Switch build that devkitPro does not package.
# Added from cmake/SwitchPlatform.cmake. Nothing here is built unless OpenMW links against it.

set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)
set(CMAKE_CXX_CLANG_TIDY "")

set(OPENMW_SWITCH_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/switch-downloads" CACHE PATH
    "Directory where source archives fetched for the Switch build are kept")
set(_deps_dir "${CMAKE_BINARY_DIR}/switch-deps")

# Downloads an archive (unless a copy with the right hash is already in the download directory) and
# extracts the given paths from it. MARKER is a file that exists once the extraction is complete.
function(openmw_switch_fetch)
    cmake_parse_arguments(PARSE_ARGV 0 FETCH "" "NAME;URL;SHA256;MARKER" "PATTERNS")
    if(EXISTS "${FETCH_MARKER}")
        return()
    endif()
    get_filename_component(_archive_name "${FETCH_URL}" NAME)
    set(_archive "${OPENMW_SWITCH_DOWNLOAD_DIR}/${_archive_name}")
    message(STATUS "Fetching ${FETCH_NAME}")
    file(DOWNLOAD "${FETCH_URL}" "${_archive}" EXPECTED_HASH SHA256=${FETCH_SHA256} STATUS _status)
    list(GET _status 0 _status_code)
    if(NOT _status_code EQUAL 0)
        message(FATAL_ERROR "Failed to download ${FETCH_URL}: ${_status}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_deps_dir}" PATTERNS ${FETCH_PATTERNS})
    if(NOT EXISTS "${FETCH_MARKER}")
        message(FATAL_ERROR "${FETCH_NAME}: ${FETCH_MARKER} is missing after extraction")
    endif()
endfunction()

# -----------------------------------------------------------------------------------------------------------
# POSIX functions missing from devkitA64's C library.

# An object library, so that it is always part of the executable: the libraries that need these functions
# only come up later on the link line, and pthread_create is wrapped rather than referenced.
add_library(nxcompat OBJECT compat/nxcompat.c)
target_include_directories(nxcompat SYSTEM PUBLIC compat/include)

# -----------------------------------------------------------------------------------------------------------
# Desktop OpenGL entry points. See glshim/nxgl.c.

find_library(OPENMW_SWITCH_EGL_LIBRARY EGL REQUIRED)
find_library(OPENMW_SWITCH_GLAPI_LIBRARY glapi REQUIRED)
find_library(OPENMW_SWITCH_DRM_NOUVEAU_LIBRARY drm_nouveau REQUIRED)

add_library(nxgl STATIC glshim/nxgl.c)
target_link_libraries(nxgl INTERFACE
    "${OPENMW_SWITCH_EGL_LIBRARY}"
    "${OPENMW_SWITCH_GLAPI_LIBRARY}"
    "${OPENMW_SWITCH_DRM_NOUVEAU_LIBRARY}"
)

# -----------------------------------------------------------------------------------------------------------
# Mesa's OpenGL calls on a worker thread. See glthread/nxglthread.c.
#
# It reaches into structures of Mesa's that are not part of any interface, so it is tied to the one build
# of Mesa it was checked against. With any other, OpenMW is built without it.

set(_mesa_checked_version "20.1.0-rc3")
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENMW_SWITCH_MESA_EGL REQUIRED egl)

add_library(nxglthread OBJECT glthread/nxglthread.c)
target_include_directories(nxglthread PUBLIC glthread)

if(OPENMW_SWITCH_MESA_EGL_VERSION STREQUAL _mesa_checked_version)
    # devkitPro's source tree for that version, for the declarations of those structures.
    set(_mesa_commit 05fc4b1d449b8fe12cf1a47b46c4a1c1f4e4e3a6)
    set(_mesa_root "${_deps_dir}/mesa-${_mesa_commit}")
    openmw_switch_fetch(
        NAME "Mesa ${_mesa_checked_version} headers (devkitPro)"
        URL "https://github.com/devkitPro/mesa/archive/${_mesa_commit}.tar.gz"
        SHA256 935a9aa82e4f0ac8a6352700849b48398f70954a97e908673d79c46fc9377f84
        MARKER "${_mesa_root}/src/egl/main/egldriver.h"
        PATTERNS
            "mesa-${_mesa_commit}/include"
            "mesa-${_mesa_commit}/src/egl/main"
            "mesa-${_mesa_commit}/src/gallium/include"
            "mesa-${_mesa_commit}/src/mapi/glapi/glapi.h"
            "mesa-${_mesa_commit}/src/util/detect_os.h"
            "mesa-${_mesa_commit}/src/util/macros.h"
            "mesa-${_mesa_commit}/src/util/u_endian.h"
    )
    target_include_directories(nxglthread SYSTEM PRIVATE
        "${_mesa_root}/include"
        "${_mesa_root}/src"
        "${_mesa_root}/src/egl/main"
        "${_mesa_root}/src/gallium/include"
    )
    # _ISOC11_SOURCE is how devkitPro has Mesa use the C library's threads instead of its own, and
    # HAVE_TIMESPEC_GET keeps Mesa's header from defining a function the C library has.
    target_compile_definitions(nxglthread PRIVATE
        NXGLTHREAD_SUPPORTED _ISOC11_SOURCE _GNU_SOURCE HAVE_TIMESPEC_GET)
    set(OPENMW_SWITCH_GLTHREAD ON PARENT_SCOPE)
else()
    message(WARNING "switch-mesa is ${OPENMW_SWITCH_MESA_EGL_VERSION}, and running its OpenGL calls on a "
        "worker thread has only been checked against ${_mesa_checked_version}: building without it. "
        "See switch/glthread/nxglthread.c.")
    set(OPENMW_SWITCH_GLTHREAD OFF PARENT_SCOPE)
endif()

# -----------------------------------------------------------------------------------------------------------
# Memory allocator: mimalloc in place of the C library's. See malloc/nxmalloc.c.

option(OPENMW_SWITCH_MIMALLOC "Replace the C library's memory allocator with mimalloc" ON)
if(OPENMW_SWITCH_MIMALLOC)
    set(_mimalloc_name mimalloc-2.1.7)
    set(_mimalloc_root "${_deps_dir}/${_mimalloc_name}")
    openmw_switch_fetch(
        NAME "mimalloc 2.1.7"
        URL "https://github.com/microsoft/mimalloc/archive/refs/tags/v2.1.7.tar.gz"
        SHA256 0eed39319f139afde8515010ff59baf24de9e47ea316a315398e8027d198202d
        MARKER "${_mimalloc_root}/src/alloc.c"
        PATTERNS
            "${_mimalloc_name}/LICENSE"
            "${_mimalloc_name}/include"
            "${_mimalloc_name}/src"
    )

    # Where mimalloc has to wait a moment for another thread, it uses the instruction "wfe" on this
    # architecture, which puts the core to sleep until its next interrupt. Linux arranges for one every
    # hundred microseconds. Here the next one can be ten milliseconds away, so use the plain hint.
    set(_mimalloc_atomic "${_mimalloc_root}/include/mimalloc/atomic.h")
    file(READ "${_mimalloc_atomic}" _atomic)
    string(REPLACE "__asm__ volatile(\"wfe\");" "__asm__ volatile(\"yield\");" _atomic_patched "${_atomic}")
    string(FIND "${_atomic_patched}" "__asm__ volatile(\"yield\");" _yield_position)
    if(_yield_position EQUAL -1)
        message(FATAL_ERROR "mimalloc: mi_atomic_yield in ${_mimalloc_atomic} is not what it was")
    elseif(NOT _atomic_patched STREQUAL _atomic)
        file(WRITE "${_mimalloc_atomic}" "${_atomic_patched}")
    endif()

    # Everything but the system layer (prim/) and the files that others include.
    set(_mimalloc_sources
        alloc.c alloc-aligned.c alloc-posix.c arena.c bitmap.c heap.c init.c libc.c options.c os.c page.c
        random.c segment.c segment-map.c stats.c
    )
    list(TRANSFORM _mimalloc_sources PREPEND "${_mimalloc_root}/src/")

    add_library(nxmalloc OBJECT ${_mimalloc_sources} malloc/nxmalloc_prim.c malloc/nxmalloc.c)
    target_include_directories(nxmalloc SYSTEM PRIVATE "${_mimalloc_root}/include")
    target_compile_definitions(nxmalloc PRIVATE MI_STATIC_LIB)
    target_compile_options(nxmalloc PRIVATE
        -include "${CMAKE_CURRENT_SOURCE_DIR}/malloc/nxmalloc_config.h"
        # Or the compiler takes the functions defined here for the ones it knows.
        -fno-builtin-malloc
        -w
    )
    set_target_properties(nxmalloc PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
endif()

# -----------------------------------------------------------------------------------------------------------
# Profile-guided optimisation: writing out the counts of a build that takes them. See pgo/nxpgo.c.

if(OPENMW_SWITCH_PGO STREQUAL "GENERATE")
    # nxpgo.c stands in for the two functions of the compiler's runtime library that count indirect
    # calls. That only works for as long as these are the two the compiler calls.
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libgcov.a
        OUTPUT_VARIABLE _libgcov OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND "${CMAKE_NM}" --defined-only "${_libgcov}"
        OUTPUT_VARIABLE _libgcov_symbols ERROR_QUIET
    )
    string(REGEX MATCHALL "__gcov_indirect_call_[a-z_]*profiler[a-z0-9_]*" _profilers "${_libgcov_symbols}")
    list(SORT _profilers)
    list(REMOVE_DUPLICATES _profilers)
    if(NOT _profilers STREQUAL "__gcov_indirect_call_profiler_v4;__gcov_indirect_call_profiler_v4_atomic")
        message(FATAL_ERROR "OPENMW_SWITCH_PGO: ${_libgcov} counts indirect calls with '${_profilers}', "
            "which is not what switch/pgo/nxpgo.c replaces. See the comment there.")
    endif()

    add_library(nxpgo OBJECT pgo/nxpgo.c)
    target_include_directories(nxpgo PUBLIC pgo)
endif()

# -----------------------------------------------------------------------------------------------------------
# Boost. Only program_options needs compiling; iostreams and geometry are used header-only.

set(_boost_name boost_1_88_0)
set(_boost_root "${_deps_dir}/${_boost_name}")
openmw_switch_fetch(
    NAME "Boost 1.88.0 (headers and program_options)"
    URL "https://archives.boost.io/release/1.88.0/source/${_boost_name}.tar.bz2"
    SHA256 46d9d2c06637b219270877c9e16155cbd015b6dc84349af064c088e9b5b12f7b
    MARKER "${_boost_root}/libs/program_options/src/variables_map.cpp"
    PATTERNS
        "${_boost_name}/boost"
        "${_boost_name}/LICENSE_1_0.txt"
        "${_boost_name}/libs/program_options/src"
)

add_library(boost_headers INTERFACE)
target_include_directories(boost_headers SYSTEM INTERFACE "${_boost_root}")
# Do not try to auto-link or to use platform facilities that do not exist here.
target_compile_definitions(boost_headers INTERFACE BOOST_ALL_NO_LIB)

file(GLOB _boost_program_options_sources "${_boost_root}/libs/program_options/src/*.cpp")
add_library(boost_program_options STATIC ${_boost_program_options_sources})
target_link_libraries(boost_program_options PUBLIC boost_headers)

add_library(Boost::headers ALIAS boost_headers)
add_library(Boost::program_options ALIAS boost_program_options)
# Only boost/iostreams/stream.hpp is used, which needs no compiled code.
add_library(Boost::iostreams ALIAS boost_headers)

# -----------------------------------------------------------------------------------------------------------
# ICU. Its own build system needs autotools and a host build of its tools to generate the data library.
# Instead the library sources are compiled directly and the prebuilt data archive that ships in the source
# tarball is loaded from the SD card at runtime (see OPENMW_SWITCH_ICU_DATA_FILE).

set(_icu_source "${_deps_dir}/icu/source")
openmw_switch_fetch(
    NAME "ICU 70.1"
    URL "https://github.com/unicode-org/icu/releases/download/release-70-1/icu4c-70_1-src.tgz"
    SHA256 8d205428c17bf13bb535300669ed28b338a157b1c01ae66d31d0d3e2d47c3fd5
    MARKER "${_icu_source}/data/in/icudt70l.dat"
    PATTERNS
        "icu/LICENSE"
        "icu/source/common"
        "icu/source/i18n"
        "icu/source/stubdata"
        "icu/source/data/in/icudt70l.dat"
)

file(GLOB _icu_common_sources "${_icu_source}/common/*.cpp")
file(GLOB _icu_i18n_sources "${_icu_source}/i18n/*.cpp")

add_library(icuuc STATIC ${_icu_common_sources})
target_include_directories(icuuc SYSTEM PUBLIC "${_icu_source}/common")
target_compile_definitions(icuuc
    PUBLIC
        U_STATIC_IMPLEMENTATION
        # The platform is not one ICU knows, so say explicitly that char* strings are UTF-8. Without this
        # ICU would go looking for converter data, which OpenMW does not ship.
        U_CHARSET_IS_UTF8=1
    PRIVATE
        U_COMMON_IMPLEMENTATION
        U_ENABLE_DYLOAD=0
        U_HAVE_MMAP=0
)

add_library(icui18n STATIC ${_icu_i18n_sources})
target_include_directories(icui18n SYSTEM PUBLIC "${_icu_source}/i18n")
target_compile_definitions(icui18n PRIVATE U_I18N_IMPLEMENTATION)
target_link_libraries(icui18n PUBLIC icuuc)

# An empty data library: the real data is read from a file.
add_library(icudata STATIC "${_icu_source}/stubdata/stubdata.cpp")
target_link_libraries(icudata PUBLIC icuuc)

set_target_properties(icuuc icui18n icudata PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)

set(ICU_LIBRARIES icui18n icuuc icudata PARENT_SCOPE)
set(ICU_INCLUDE_DIRS "${_icu_source}/common" "${_icu_source}/i18n" PARENT_SCOPE)
set(OPENMW_SWITCH_ICU_DATA_FILE "${_icu_source}/data/in/icudt70l.dat" PARENT_SCOPE)
