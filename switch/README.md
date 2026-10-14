# OpenMW for Nintendo Switch (homebrew)

An unofficial port of OpenMW to Nintendo Switch homebrew, built with devkitPro. It is not supported by the
OpenMW project.

## Building

Requirements:

- devkitPro with the Switch toolchain (`switch-dev`), CMake and make.
- These packages, installed with `pacman -S` (`dkp-pacman -S` on Linux/macOS):

      switch-sdl2 switch-mesa switch-ffmpeg switch-openal-soft switch-lz4 switch-freetype
      switch-libpng switch-libjpeg-turbo switch-liblua51 switch-bzip2 switch-zlib

Then, from devkitPro's MSYS2 shell on Windows (or any shell with `DEVKITPRO` set elsewhere):

    switch/build.sh

Everything else (OpenSceneGraph, MyGUI, Bullet, Recast, Boost, ICU, SQLite, yaml-cpp) is downloaded and
built as part of the OpenMW build. The first build takes a long time.

## Working on the code in CLion

CLion cannot configure this build itself: it would run CMake as a Windows program, and devkitPro's
libraries are set up for its MSYS2 shell. Instead CLion reads the compile commands of the build that
`build.sh` set up, and runs `build.sh` to build.

1. Build once with `switch/build.sh`. Every run also writes `build-switch/clion/compile_commands.json`:
   the compile commands of OpenMW's own files, with Windows paths.
2. In CLion, **File | Open** that file and choose **Open as Project**. Then **Tools | Compilation
   Database | Change Project Root** to the top of the repository.
3. In **Settings | Build, Execution, Deployment | Custom Build Targets**, add a target whose **Build** is
   an external tool with the program `C:\devkitPro\msys2\usr\bin\bash.exe` and the arguments
   `-l /c/path/to/openmw/switch/build.sh` (the script's path the way MSYS2 spells it). Leave **Clean**
   empty: after a clean, all the libraries are built again. **Build | Build Project** then does what
   `switch/build.sh` does.
4. To send each build to the console: a **Custom Build Application** run configuration with that target,
   the executable `C:\devkitPro\tools\bin\nxlink.exe`, the arguments
   `-s -p switch/openmw/openmw.nro build-switch/openmw.nro` and the repository as working directory. With
   the homebrew menu started by holding R and its network loader open (Y), **Run** builds, replaces
   `switch/openmw/openmw.nro` on the card, starts it and shows its log until it quits.

Files added to or removed from the build appear in CLion after the next build. In a build directory set
up for profile-guided optimisation, functions changed since the profile was taken are compiled without
it, each with a warning.

## Installing

1. Copy the contents of `build-switch/sdcard` to the root of the SD card. This creates `switch/openmw`.
   Nothing in it is user data, so the same step updates an existing installation.
2. Copy the contents of Morrowind's `Data Files` folder to `switch/openmw/data` on the card.
3. Copy `Morrowind.ini` from the Morrowind installation folder to `switch/openmw`.
4. Start the homebrew menu **by holding R while launching a game**, then start OpenMW. Opening the homebrew
   menu from the album does not give OpenMW enough memory.

On first start OpenMW writes `switch/openmw/config/openmw.cfg`: it lists Morrowind and whichever of
Tribunal and Bloodmoon are present, and imports the game settings from `Morrowind.ini`. Delete that file to
have it written again. Saves are in `switch/openmw/saves`.

Paths in `openmw.cfg` must be written without the `sdmc:` prefix, for example
`data="/switch/openmw/mods/something"`.

The log is written to `switch/openmw/config/openmw.log`. When OpenMW is started with `nxlink -s openmw.nro`,
the log is also sent to the PC.

## Giving a build to others

A zip with the `switch` folder from `build-switch/sdcard`, `INSTALL.txt` from this directory (the
install guide for players) and OpenMW's `LICENSE` as `LICENSE.txt`, all three at the top of the zip.
Nothing in `build-switch/sdcard` is user data or comes from Morrowind; players bring their own game
files. OpenMW is under the GPL version 3, so whoever receives the build is entitled to its source code.

## Display and controller

The game is drawn at 1280x720 on the console's own screen and at 1920x1080 while the console is in its
dock, and changes over when it is put in or taken out. The resolution in the settings is not a choice
here. The interface is laid out for the console's own screen and drawn one and a half times as large in
the dock, so it takes up the same part of the picture on both. Its text is still prepared for 720 lines
and is not as sharp on a television as the rest.

Buttons go by what is written on them: where the game says A it means the button marked A, as it does
with a Nintendo controller on a PC.

## Environment variables and profiling

The homebrew menu cannot pass environment variables, so OpenMW reads them from an optional file,
`switch/openmw/environment.txt`: one `NAME=value` per line, lines starting with `#` are ignored. Anything
OpenMW, OpenSceneGraph or Mesa reads from the environment can be set there.

    # Show the frame rate. 2 adds the profiler bars, as pressing F3 twice does on a PC.
    OPENMW_SWITCH_SHOW_STATS=1

    # Record per-frame timings, to be analysed on a PC with scripts/osg_stats.py.
    OPENMW_OSG_STATS_FILE=/switch/openmw/osg_stats.txt
    OPENMW_OSG_STATS_LIST=times

    # Sample what the main, drawing and worker threads are doing, to be read on a PC with
    # switch/profile.py and the openmw.elf of the same build.
    OPENMW_SWITCH_PROFILE=/switch/openmw/profile.bin

    # Mesa runs OpenGL calls on a thread of its own, see "Performance". This turns that off.
    mesa_glthread=false

    # Have OpenSceneGraph check for OpenGL errors again and report them in the log. Off by default,
    # because every check has the thread that draws wait for Mesa's.
    OSG_GL_ERROR_CHECKING=ONCE_PER_FRAME

Recording slows the game down a little, and the timings file grows by several megabytes a minute. Both
files are written when the game is quit from its own menu.

## Performance

The game has three cores at 1020 MHz. Measured in Seyda Neen with the tools above, a frame costs about
100 ms of CPU time across all threads, and the thread that draws sets the frame rate: around 1300 draw
calls a frame at 30 to 40 µs each, a good half of that inside Mesa and a third in OpenSceneGraph's state
and uniform handling. The main thread spends a fifth of its time in the C library's memory allocator,
which has a single lock that all threads queue for.

Default settings that differ from the desktop ones for this reason are listed with their measurements
in `cmake/SwitchPackage.cmake`: no transparent post-pass, the `shaders` lighting method, fewer Lua
garbage collection steps per frame, a viewing distance of 5081 in place of 7168. All of them can be
changed back in `config/settings.cfg`, and the viewing distance in the game's own settings.

Mesa can run the OpenGL calls it is given on a worker thread ("glthread"), which takes its half of the
work off the thread that draws. devkitPro's Mesa contains the code but has no way to turn it on;
`glthread/nxglthread.c` adds that around the Switch EGL driver's functions. It depends on the exact build
of Mesa, so `CMakeLists.txt` only builds it for the version it was checked against. It is on unless
`environment.txt` says `mesa_glthread=false`. With it the main thread became the limit.

The C library's memory allocator has a single lock, and with five or six threads allocating, the main
thread spent a third to half of its time in it, much of that waiting. `malloc/` replaces it with
[mimalloc](https://github.com/microsoft/mimalloc), which gives every thread memory of its own to
allocate from. `-DOPENMW_SWITCH_MIMALLOC=OFF` builds with the C library's allocator again.

Profile-guided optimisation (next section) left the average frame rate about where it was and made it
much steadier, with most of the remaining stutter gone. What can be seen in the executable: the functions
that run every frame sit within about 1.2 MB of it instead of being spread over 42 MB. Splitting the
rarely run parts off those functions as well (`-freorder-blocks-and-partition`, which the compiler does
not do by default on this architecture) was measured on four of the busiest source files and takes
another 0 to 3 % out of the hot code of three of them and 17 % out of the fourth, so it is not turned on.

Where that leaves it, measured in Seyda Neen and Balmora with all of the above: a frame costs 60 to 73 ms
of CPU time, which is 2.5 to 2.6 of the three cores. Mesa's thread has the largest share with 31 %, then
the main thread with 26 to 28 %, the thread that draws with 25 % and the physics and Lua workers with the
rest. No thread is the limit by itself any more. The thread that draws waits for room in Mesa's queue for
30 % of the time and Mesa's thread waits for work for 20 %: the queue holds seven buffers of 8 KB
(`MARSHAL_MAX_BATCHES` in Mesa's `glthread.h`, a part of devkitPro's library), a few milliseconds of
work, so the two cannot each go at their own pace. Replaying the recording with a queue without that
limit gives 3 to 15 % more frames per second, and three cores in full use would give 14 to 19 %.

Beyond that, only less work per frame helps, and that is a matter of how much is in view: the frame time
is 8 to 9 ms plus 15 µs for every object drawn. At a field of view of 90 and the full viewing distance
that was 1250 to 1500 objects on average and over 2200 in the densest views of Balmora. The field of view
counts most, since the area in view grows with the tangent of half of it (60, the default, shows 58 % of
what 90 does). Lowering the viewing distance from 7168 to 4635 took a fifth of the objects and a tenth
of the frame time out of one view of Balmora.

## Profile-guided optimisation

Optional. The compiler can arrange and optimise the code by how it actually runs on the console instead
of by its own guesses: which branches are taken, which functions call which, what is hot and what never
runs. It takes two full builds with a session on the console in between.

1. Build an executable that counts what it executes:

       switch/build.sh --pgo=generate

2. Copy `build-switch/sdcard` to the card as usual and play for a while, the way the game is normally
   played: walk through a town, go indoors, talk, trade, fight, cast a spell, open the map and the
   inventory, load a save. This build is slow. Then quit from the game's own menu, which is when it writes
   `switch/openmw/pgo-profile.gcfn`. Closing it from the HOME menu loses the counts.

3. Copy that file to the PC and build with it:

       switch/build.sh --pgo=use --profile=path/to/pgo-profile.gcfn

   `--profile` can be given more than once, to add up the counts of several sessions.

The build directory then stays in this mode, and plain `switch/build.sh` keeps using the counts.
`--pgo=off` goes back.

- The counts belong to one build directory: steps 1 and 3 have to use the same one, in the same place.
  Keep the `.gcfn` file, with it step 3 can be repeated after the build directory was deleted.
- Code that did not run during the session is compiled as it is without all this, not treated as
  unimportant, so a short session costs nothing elsewhere.
- A function that was changed after the counts were taken is compiled without them, with a warning that
  its profile data is out of date. After larger changes, start over from step 1.
- What devkitPro ships prebuilt (Mesa, SDL, FFmpeg, OpenAL, Lua) is not affected, nor are the libraries
  built in `switch/CMakeLists.txt`.

How it works here: the compiler's runtime library would write one file of counts per source file into
the build directory when the program exits, and that directory is on the PC. So the counts are collected
the way the compiler provides for systems without a file system (`-fprofile-info-section`), and
`pgo/nxpgo.c` writes them to one file when the game exits. `build.sh` has the compiler's `gcov-tool`
split that into the per-source files, and `pgo/summary.pl` fills in the one thing the tool leaves out.
`nxpgo.c` also stands in for two functions of that runtime library which devkitPro builds in a way that
does not work on this system; they are only part of the counting build.

## How the port is put together

- `cmake/SwitchToolchain.cmake`, `cmake/SwitchPlatform.cmake`, `cmake/SwitchPackage.cmake`: build setup,
  hooked into the top-level `CMakeLists.txt` behind `NINTENDO_SWITCH`.
- `glshim/`: Mesa on Switch exports no desktop OpenGL entry points, so this library provides them by
  forwarding to `eglGetProcAddress`. `nxgl_stubs.inc` is generated by `generate.py` from the Mesa headers.
- `glthread/`: what devkitPro's Mesa lacks to run OpenGL calls on a worker thread, see "Performance".
- `malloc/`: mimalloc as the program's memory allocator: the layer it needs for this system, and the
  C library's allocation functions defined in terms of it.
- `pgo/`: for profile-guided optimisation, see above: writing the counts out on the console, and
  completing them on the PC.
- `compat/`: the few POSIX functions that devkitA64's C library lacks (`dlopen` and friends, `sysconf`),
  and wrappers for three that behave differently: `pthread_create` (128 KiB default stack), `rename`
  (does not replace an existing file) and SDL's thread priorities (audio thread left at the lowest).
- `CMakeLists.txt`: builds the above, plus Boost.ProgramOptions and ICU, which devkitPro does not package.
- `sdcard/`: configuration files shipped on the SD card.
- Changes to OpenMW itself are guarded by `__SWITCH__`.
- Docking: SDL would resize the window by itself, from the main thread, which replaces the surface that
  the thread that draws is drawing to. So the window is not created as resizable, and
  `SDLUtil::InputWrapper` changes its size with that thread stopped, the way OpenMW changes the vertical
  synchronisation. From there on it is a window that was resized, which OpenMW knows how to follow.
- Threads: libnx starts every thread but the main one at the lowest priority. The thread that draws and
  Mesa's worker thread are raised to the priority of the main thread, the physics, Lua and music workers
  to just below it (`Platform::Switch::setCurrentThreadPriority`), SDL's audio thread to above it, and
  everything else stays where it is.
