# Final link adjustments and packaging for the Nintendo Switch build.
# Included from the top-level CMakeLists.txt once the openmw target exists.

# nxcompat stands in front of three functions: pthread_create, to raise libnx's 128 KiB default stack
# size; SDL's choice of thread priority, which leaves its audio thread at the lowest one; and rename,
# which does not replace an existing file here.
target_sources(openmw PRIVATE $<TARGET_OBJECTS:nxcompat>)
target_link_options(openmw PRIVATE
    "-Wl,--wrap=pthread_create"
    "-Wl,--wrap=SDL_SYS_SetThreadPriority"
    "-Wl,--wrap=rename"
)

# The memory allocator. As objects for the same reason: nothing refers to them by any name the C library
# does not have as well.
if(TARGET nxmalloc)
    target_sources(openmw PRIVATE $<TARGET_OBJECTS:nxmalloc>)
endif()

# nxglthread stands in front of the function that sets up Mesa's EGL driver, to add what it takes to run
# OpenGL calls on a worker thread, and in front of thrd_create, whose result Mesa misreads.
target_sources(openmw PRIVATE $<TARGET_OBJECTS:nxglthread>)
if(OPENMW_SWITCH_GLTHREAD)
    target_link_options(openmw PRIVATE "-Wl,--wrap=_eglInitDriver" "-Wl,--wrap=thrd_create")
endif()

# In a build that counts what it executes: the function that writes the counts out, and the compiler's
# runtime library for the counting. As objects so that the two functions nxpgo has in place of that
# library's are found first.
if(TARGET nxpgo)
    target_sources(openmw PRIVATE $<TARGET_OBJECTS:nxpgo>)
    target_link_libraries(openmw gcov)
endif()

# The packaged libraries are static, so whatever they depend on has to be named here. pkg-config knows.
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENMW_SWITCH_STATIC REQUIRED
    libavformat libavcodec libswscale libswresample libavutil
    openal
    freetype2 libpng
)
target_link_directories(openmw PRIVATE ${OPENMW_SWITCH_STATIC_STATIC_LIBRARY_DIRS})
target_link_libraries(openmw ${OPENMW_SWITCH_STATIC_STATIC_LIBRARIES})

# Default settings that differ on the console. They are baked into defaults.bin rather than shipped as a
# settings.cfg, so that updating the installation on the SD card never overwrites the user's own settings.
set(_setting_overrides
    # What the settings start out with. The size drawn at is that of the console's own screen or, in the
    # dock, 1920x1080, whatever they say: see Platform::Switch::getDisplaySize.
    "resolution x=1280"
    "resolution y=720"
    # Vertical sync on: rendering faster than the display only drains the battery.
    "vsync mode=1"
    "controller menus=true"
    # The navigation mesh cache is an SQLite database, which is slow on an SD card.
    "enable nav mesh disk cache=false"
    "write to navmeshdb=false"
    # The three below come out of profiling on the console, see README.md.
    # Draws every transparent object a second time, for post-processing shaders that are off by default.
    # It took a seventh of the time of the thread that draws.
    "transparent postpass=false"
    # "shaders compatibility" builds a new set of light uniforms for every lit object in every frame.
    # "shaders" keeps the lights in one buffer and reuses the per-object state from frame to frame.
    "lighting method=shaders"
    # Tuned for LuaJIT on a desktop. With plain Lua here, three fifths of the scripting thread's time
    # went into stepping the garbage collector. It still collects on its own as memory is allocated.
    "gc steps per frame=25"
    # With this and the field of view left at 60, towns hold 60 frames per second on the console's own
    # screen. The frame time follows the number of objects in view, see README.md.
    "viewing distance=5081"
)
file(READ "${OpenMW_SOURCE_DIR}/files/settings-default.cfg" _settings)
foreach(_override IN LISTS _setting_overrides)
    string(REGEX MATCH "^([^=]+)=(.*)$" _match "${_override}")
    set(_name "${CMAKE_MATCH_1}")
    set(_value "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "(\n${_name} = )[^\n]*" "\\1${_value}" _replaced "${_settings}")
    if(_replaced STREQUAL _settings)
        message(FATAL_ERROR "Switch default for '${_name}': no such setting in files/settings-default.cfg")
    endif()
    set(_settings "${_replaced}")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/switch/settings-default.cfg" "${_settings}")
pack_resource_file("${CMAKE_BINARY_DIR}/switch/settings-default.cfg" "${OpenMW_BINARY_DIR}" "defaults.bin")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OpenMW_SOURCE_DIR}/files/settings-default.cfg")

# openmw.nro
nx_generate_nacp(
    OUTPUT "${CMAKE_BINARY_DIR}/openmw.nacp"
    NAME "OpenMW"
    AUTHOR "OpenMW contributors"
    VERSION "${OPENMW_VERSION}"
)
nx_create_nro(openmw
    OUTPUT "${CMAKE_BINARY_DIR}/openmw.nro"
    NACP "${CMAKE_BINARY_DIR}/openmw.nacp"
    ICON "${CMAKE_SOURCE_DIR}/switch/icon.jpg"
)

# A folder laid out the way it goes onto the SD card: copy the contents of sdcard/ to the card's root.
# Nothing in it is user data, so it can be copied over an existing installation.
set(_sd_root "${CMAKE_BINARY_DIR}/sdcard")
set(_sd_dir "${_sd_root}/switch/openmw")
add_custom_target(openmw_sdcard ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_sd_dir}/data"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_BINARY_DIR}/openmw.nro"
        "${OpenMW_BINARY_DIR}/openmw.cfg"
        "${OpenMW_BINARY_DIR}/defaults.bin"
        "${OpenMW_BINARY_DIR}/gamecontrollerdb.txt"
        "${OPENMW_SWITCH_ICU_DATA_FILE}"
        "${_sd_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OpenMW_BINARY_DIR}/resources" "${_sd_dir}/resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/switch/sdcard/data/README.txt"
        "${_sd_dir}/data"
    COMMENT "Assembling SD card folder in ${_sd_root}"
    VERBATIM
)
add_dependencies(openmw_sdcard openmw_nro)
