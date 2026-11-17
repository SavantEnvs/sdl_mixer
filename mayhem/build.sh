#!/usr/bin/env bash
#
# mayhem/build.sh — build SDL3_mixer with every decoder (in-tree and external) and the fuzz flags,
# the libFuzzer harnesses + their -standalone reproducers, and a separate normal-flags build for
# mayhem/test.sh. SDL3 and the external decoder libraries (libxmp, libgme, wavpack, mpg123,
# fluidsynth built static from the pinned SDL_mixer submodule commits; ogg/vorbis/FLAC/opus/opusfile
# from apt's static libs) are prebuilt by the Dockerfile, uninstrumented, under
# /opt/toolchains/deps/<name>-<version>. Offline and re-runnable.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SDL3_PREFIX:=/opt/toolchains/deps/sdl3-3.4.0}"
export CC CXX

SRC="${SRC:-/mayhem}"
cd "$SRC"

# Sanitizer coverage on the LIBRARY too (not just the harness), so the fuzzer sees the decoders.
COV=""
case "$LIB_FUZZING_ENGINE" in *fuzzer*) COV="-fsanitize=fuzzer-no-link" ;; esac

# One UBSan check is relaxed because a benign idiom trips it on the first seed and would hide every
# other finding: shift-base (timidity's `sample << GUARD_BITS` on negative PCM, dr_flac's
# `byte << 24` into int). pointer-overflow stays ON: stb_vorbis's `NULL + 0` at stb_vorbis.h:4368
# (`p->comment_list[i]` in vorbis_deinit, comment_list NULL with a positive length) is the first
# symptom of a real NULL dereference, not a benign idiom. ASan and the rest of UBSan stay halting.
UB_RELAX=""
case "$SANITIZER_FLAGS" in *undefined*) UB_RELAX="-fno-sanitize=shift-base" ;; esac
SANITIZER_FLAGS="$SANITIZER_FLAGS $UB_RELAX"

# Every dep prefix under /opt/toolchains/deps, for CMake's Find modules and pkg-config.
PREFIXES=""; PKGCFG=""
for d in /opt/toolchains/deps/*/; do
  d="${d%/}"; PREFIXES="${PREFIXES:+$PREFIXES;}$d"
  for l in lib lib64 lib/x86_64-linux-gnu; do [ -d "$d/$l/pkgconfig" ] && PKGCFG="${PKGCFG:+$PKGCFG:}$d/$l/pkgconfig"; done
done
export PKG_CONFIG_PATH="$PKGCFG${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

MIXER_OPTS=(
  -G Ninja
  -DCMAKE_PREFIX_PATH="$PREFIXES"
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="$SRC/mayhem/static_libs.cmake"
  -DSDLMIXER_STRICT=ON
  -DBUILD_SHARED_LIBS=OFF
  -DSDLMIXER_VENDORED=OFF -DSDLMIXER_DEPS_SHARED=OFF
  -DSDLMIXER_TESTS=OFF -DSDLMIXER_EXAMPLES=OFF -DSDLMIXER_INSTALL=OFF
  -DSDLMIXER_FLAC_LIBFLAC=ON -DSDLMIXER_GME=ON -DSDLMIXER_MOD=ON -DSDLMIXER_MOD_XMP=ON
  -DSDLMIXER_MP3_MPG123=ON -DSDLMIXER_MIDI_FLUIDSYNTH=ON -DSDLMIXER_OPUS=ON
  -DSDLMIXER_VORBIS_VORBISFILE=ON -DSDLMIXER_WAVPACK=ON
)

# 1) The library, instrumented (ASan+UBSan halting + SanCov), DWARF 3. SDL_assert stays compiled in:
# SDL_assert.h drops to level 1 (SDL_assert a no-op) under -O1 unless SDL_DEFAULT_ASSERT_LEVEL is set,
# which would also disable dr_mp3/dr_flac's DRMP3_ASSERT/DRFLAC_ASSERT. The harnesses set
# SDL_HINT_ASSERT=abort so a failed assert aborts instead of prompting.
ASSERT_FLAGS="-DSDL_DEFAULT_ASSERT_LEVEL=2"
cmake -S . -B build-fuzz "${MIXER_OPTS[@]}" -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $COV $DEBUG_FLAGS $ASSERT_FLAGS -O1"
cmake --build build-fuzz -j"$MAYHEM_JOBS"

# The decoder libraries' own static link line, from their pkg-config files (ordered, with their deps).
# shellcheck disable=SC2207
EXTRA=($(pkg-config --static --libs opusfile vorbisfile flac libxmp libgme wavpack libmpg123 fluidsynth \
  | tr ' ' '\n' | grep -v '^-l\(m\|pthread\|dl\|stdc++\|gcc_s\|gcc\|c\|rt\)$' | awk '!s[$0]++'))
LIBS=("$SRC/build-fuzz/libSDL3_mixer.a" -Wl,-Bstatic -Wl,--start-group "${EXTRA[@]}" -Wl,--end-group -Wl,-Bdynamic "$SDL3_PREFIX/lib/libSDL3.a" -lm -lpthread -ldl -lstdc++)
INC=(-I"$SRC/include" -I"$SDL3_PREFIX/include")
H="$SRC/mayhem/harnesses"
CFL="$SANITIZER_FLAGS $DEBUG_FLAGS $ASSERT_FLAGS -O1"

mkdir -p build-fuzz/obj
# shellcheck disable=SC2086
$CC $CFL -c "$SRC/mayhem/lsan_off.c" -o build-fuzz/obj/lsan_off.o
# shellcheck disable=SC2086
$CC $CFL -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/obj/standalone_main.o

# name  source  extra-defines
TARGETS=(
  "sdl_mixer_fuzzer   fuzz_load_music_from_mem.c"
  "fuzz_wav           fuzz_decode.c -DDECODER_NAME=\"WAV\""
  "fuzz_stbvorbis     fuzz_decode.c -DDECODER_NAME=\"STBVORBIS\""
  "fuzz_voc           fuzz_decode.c -DDECODER_NAME=\"VOC\""
  "fuzz_mixer         fuzz_mixer.c"
  "fuzz_drmp3         fuzz_decode.c -DDECODER_NAME=\"DRMP3\""
  "fuzz_drflac        fuzz_decode.c -DDECODER_NAME=\"DRFLAC\""
  "fuzz_aiff          fuzz_decode.c -DDECODER_NAME=\"AIFF\""
  "fuzz_au            fuzz_decode.c -DDECODER_NAME=\"AU\""
  "fuzz_timidity_midi fuzz_timidity.c -DMODE_MIDI"
  "fuzz_timidity_sf2  fuzz_timidity.c -DMODE_SF2"
  "fuzz_timidity_pat  fuzz_timidity.c -DMODE_PAT"
  "fuzz_flac          fuzz_decode.c -DDECODER_NAME=\"FLAC\""
  "fuzz_vorbis        fuzz_decode.c -DDECODER_NAME=\"VORBIS\""
  "fuzz_opus          fuzz_decode.c -DDECODER_NAME=\"OPUS\""
  "fuzz_mpg123        fuzz_decode.c -DDECODER_NAME=\"MPG123\""
  "fuzz_xmp           fuzz_decode.c -DDECODER_NAME=\"XMP\""
  "fuzz_gme           fuzz_decode.c -DDECODER_NAME=\"GME\""
  "fuzz_wavpack       fuzz_decode.c -DDECODER_NAME=\"WAVPACK\""
  "fuzz_fluidsynth    fuzz_fluidsynth.c"
)
for t in "${TARGETS[@]}"; do
  read -r name src defs <<<"$t"
  defs="${defs:-}"
  # shellcheck disable=SC2086
  $CC $CFL $COV "${INC[@]}" $defs -c "$H/$src" -o "build-fuzz/obj/$name.o"
  # shellcheck disable=SC2086
  $CC $CFL $LIB_FUZZING_ENGINE "build-fuzz/obj/$name.o" build-fuzz/obj/lsan_off.o "${LIBS[@]}" -o "$SRC/$name"
  # shellcheck disable=SC2086
  $CC $CFL build-fuzz/obj/standalone_main.o "build-fuzz/obj/$name.o" build-fuzz/obj/lsan_off.o "${LIBS[@]}" -o "$SRC/$name-standalone"
done

# 2) Normal-flags build for the functional oracle (mayhem/test.sh runs build-tests/selftest).
# shellcheck disable=SC2086
cmake -S . -B build-tests "${MIXER_OPTS[@]}" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS"
cmake --build build-tests -j"$MAYHEM_JOBS"
# shellcheck disable=SC2086
$CC -O2 $COVERAGE_FLAGS "${INC[@]}" "$SRC/mayhem/test/selftest.c" \
  "$SRC/build-tests/libSDL3_mixer.a" -Wl,-Bstatic -Wl,--start-group "${EXTRA[@]}" -Wl,--end-group -Wl,-Bdynamic "$SDL3_PREFIX/lib/libSDL3.a" \
  -lm -lpthread -ldl -lstdc++ -o build-tests/selftest
