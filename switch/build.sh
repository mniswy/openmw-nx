#!/bin/bash
# Configures and builds OpenMW for Nintendo Switch.
#
# Run it from devkitPro's MSYS2 shell on Windows, or any shell with DEVKITPRO set on Linux/macOS:
#     switch/build.sh [options] [build directory]
#
# The result is <build directory>/sdcard, whose contents go to the root of the SD card. On Windows it also
# writes <build directory>/clion/compile_commands.json, for working on the code in CLion (see README.md).
#
# Options, all for profile-guided optimisation (see README.md):
#     --pgo=generate   Build an executable that counts what it executes.
#     --pgo=use        Build an executable optimised by those counts.
#     --pgo=off        Build without either.
#     --profile=FILE   With --pgo=use: a pgo-profile.gcfn written by the counting executable. Give it once
#                      per file to combine several. Without it, the counts already in the build directory
#                      are used.
# A build directory stays the way --pgo last set it.

set -e

source_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir=
pgo=
profiles=()
for argument in "$@"; do
    case "$argument" in
        --pgo=generate | --pgo=use | --pgo=off) pgo="${argument#--pgo=}" ;;
        --profile=*) profiles+=("${argument#--profile=}") ;;
        -*)
            echo "Unknown option: $argument" >&2
            exit 1
            ;;
        *) build_dir="$argument" ;;
    esac
done
build_dir="${build_dir:-$source_dir/build-switch}"
jobs="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

if [ -z "$DEVKITPRO" ]; then
    echo "DEVKITPRO is not set. Use the devkitPro MSYS2 shell, or install devkitPro and source its environment." >&2
    exit 1
fi

pgo_options=()
if [ -n "$pgo" ]; then
    pgo_options+=("-DOPENMW_SWITCH_PGO=$(echo "$pgo" | tr '[:lower:]' '[:upper:]')")
fi

if [ ${#profiles[@]} -gt 0 ]; then
    if [ "$pgo" != use ]; then
        echo "--profile only goes with --pgo=use." >&2
        exit 1
    fi
    for profile in "${profiles[@]}"; do
        if [ ! -f "$profile" ]; then
            echo "No such file: $profile" >&2
            exit 1
        fi
    done
    # The compiler looks for the counts of each source file next to its object file, where the counting
    # build was told they would be. Put the given ones there in place of any others. Nothing tells make
    # that the object files depend on them, so have everything compiled again.
    if [ -f "$build_dir/Makefile" ]; then
        cmake --build "$build_dir" --target clean
    fi
    find "$build_dir" -name '*.gcda' -delete
    for profile in "${profiles[@]}"; do
        "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcov-tool" merge-stream "$profile"
    done
    if [ -z "$(find "$build_dir" -name '*.gcda' -print -quit)" ]; then
        echo "The profiles hold no counts for $build_dir. They have to come from an executable built in" >&2
        echo "this same directory with --pgo=generate." >&2
        exit 1
    fi
    perl "$source_dir/switch/pgo/summary.pl" ${#profiles[@]} "$build_dir"
fi

cmake -S "$source_dir" -B "$build_dir" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$source_dir/switch/cmake/SwitchToolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_LAUNCHER=OFF \
    -DBUILD_WIZARD=OFF \
    -DBUILD_OPENCS=OFF \
    -DBUILD_MWINIIMPORTER=OFF \
    -DBUILD_ESSIMPORTER=OFF \
    -DBUILD_BSATOOL=OFF \
    -DBUILD_ESMTOOL=OFF \
    -DBUILD_NIFTEST=OFF \
    -DBUILD_NAVMESHTOOL=OFF \
    -DBUILD_BULLETOBJECTTOOL=OFF \
    -DOPENMW_USE_SYSTEM_BULLET=OFF \
    -DOPENMW_USE_SYSTEM_OSG=OFF \
    -DOPENMW_USE_SYSTEM_MYGUI=OFF \
    -DOPENMW_USE_SYSTEM_SQLITE3=OFF \
    -DOPENMW_USE_SYSTEM_YAML_CPP=OFF \
    -DOPENMW_USE_SYSTEM_ICU=OFF \
    -DUSE_LUAJIT=OFF \
    "${pgo_options[@]}"

# The compile commands name files the way this shell does. Editors running on Windows get a copy with
# Windows paths. Before building, so that it is there even when the build fails.
if command -v cygpath > /dev/null; then
    build_path="$(cd "$build_dir" && pwd)"
    mkdir -p "$build_dir/clion"
    perl "$source_dir/switch/compile_commands.pl" "$source_dir" "$build_path" "$build_dir/clion/compile_commands.json" \
        "$source_dir=$(cygpath -m "$source_dir")" \
        "$build_path=$(cygpath -m "$build_path")" \
        "$DEVKITPRO=$(cygpath -m "$DEVKITPRO")"
fi

cmake --build "$build_dir" -- -j"$jobs"

echo
echo "Done. Copy the contents of $build_dir/sdcard to the root of the SD card."
