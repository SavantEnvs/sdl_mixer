# syntax=docker/dockerfile:1.7
#
# Commit-image Dockerfile (SMALL form, PREFERRED) — builds ON TOP of the org C/C++ base
# image (ghcr.io/savantenvs/base). The base already provides debian + clang/llvm +
# libFuzzer/afl++ + the identities + the build-contract ENV, so this only adds the
# per-commit build (built on top of the prebuilt base, rather than building the toolchain in-image).
#
# Build (from the repo root):
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile \
#       --build-arg COMMIT=$(git rev-parse HEAD) \
#       [--build-arg SOURCE_DATE_EPOCH=$(date +%s)] \
#       -t mytag .

# The org base, pinned to a fixed DATE-tagged base (YYYYMMDD; shared by all integrations
# minted against it). Use the current dated tag; pin by digest in production: ...base@sha256:<digest>.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
# SOURCE_DATE_EPOCH stays an ARG (available to build.sh); do NOT bake it as an ENV — an
# empty value breaks clang ("must be a non-negative decimal integer") both here and for
# any in-image rebuild. build.sh unsets it when empty.
ARG SOURCE_DATE_EPOCH

# Sanitizer override: the base exports SANITIZER_FLAGS (ASan+UBSan, halting) as the default.
# To change it for a build (e.g. ASan-only, or add MSan): --build-arg SANITIZER_FLAGS="...".
# Declare it WITHOUT a default (no '='): an undefaulted ARG lets the base ENV flow through to
# build.sh when not passed, and an explicit --build-arg overrides it. (An empty default — `=` —
# would instead shadow the base ENV and silently wipe the sanitizers.)
ARG SANITIZER_FLAGS
# Coverage (optional, off by default): instrument the TEST build to measure how much of the project
# the suite exercises — --build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping".
# Empty by default (no effect on the normal oracle build); build.sh appends it to the test build only.
ARG COVERAGE_FLAGS=
# Build parallelism (optional). build.sh falls back to nproc when unset: --build-arg MAYHEM_JOBS=8
ARG MAYHEM_JOBS=

# SDL3 is a required external dependency of SDL3_mixer (find_package(SDL3 3.4.0)), not vendored and
# not in Debian at this version: build it once from the pinned release tarball into a static,
# uninstrumented prefix under /opt/toolchains/deps (SPEC §6.2 item 19). Only the subsystems SDL_mixer
# needs (audio + IO + properties); no video/input backends, no real audio backends (dummy driver).
USER root
ARG SDL3_VER=3.4.0
ARG SDL3_SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8
RUN d=/opt/toolchains/deps/sdl3-${SDL3_VER} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz \
      https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VER}/SDL3-${SDL3_VER}.tar.gz \
 && echo "${SDL3_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && cmake -S "$d/src" -B /tmp/dep-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_FLAGS="-O2 -g -gdwarf-3" -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_INSTALL_LIBDIR=lib \
      -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_UNIX_CONSOLE_BUILD=ON \
      -DSDL_VIDEO=OFF -DSDL_RENDER=OFF -DSDL_GPU=OFF -DSDL_CAMERA=OFF -DSDL_JOYSTICK=OFF \
      -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_DIALOG=OFF -DSDL_TRAY=OFF \
      -DSDL_ALSA=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_OSS=OFF \
 && cmake --build /tmp/dep-build -j"$(nproc)" && cmake --install /tmp/dep-build \
 && cd / && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d"

# The other decoders' libraries. apt ships static libs (and so is used, uninstrumented) for the Ogg
# family; for the rest apt has only shared libs (libxmp/libgme/libwavpack/libmpg123/libfluidsynth), so
# each is built static from the exact libsdl-org commit SDL_mixer pins as its external/ submodule, into
# /opt/toolchains/deps/<name>-<version> (SPEC §6.2 item 19). The submodules are empty in the checkout.
RUN apt-get update -qq \
 && apt-get install -y -qq --no-install-recommends libogg-dev libvorbis-dev libflac-dev libopus-dev libopusfile-dev \
 && rm -rf /var/lib/apt/lists/*
RUN set -eu; \
 dep() { \
  name=$1; ver=$2; repo=$3; commit=$4; sha=$5; sub=$6; shift 6; \
  d=/opt/toolchains/deps/${name}-${ver}; \
  mkdir -p "$d/src" /tmp/dep-build; \
  curl -fsSL -o /tmp/dep.tgz "https://github.com/libsdl-org/${repo}/archive/${commit}.tar.gz"; \
  echo "${sha}  /tmp/dep.tgz" | sha256sum -c -; \
  tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src"; \
  cmake -S "$d/src/${sub}" -B /tmp/dep-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS="-O2 -g -gdwarf-3" -DCMAKE_CXX_FLAGS="-O2 -g -gdwarf-3" \
    -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON "$@"; \
  cmake --build /tmp/dep-build -j"$(nproc)"; \
  cmake --install /tmp/dep-build; \
  rm -rf /tmp/dep-build /tmp/dep.tgz; \
 }; \
 dep libxmp 4.7.4 libxmp 3a1c88f3bb4d1ac1f5bbba4963e9109c056d2a39 b82b1a00e21e25e531b2f7f996e7fd418bc389dc64940ccc0257b5263195db4f . \
     -DBUILD_STATIC=ON -DBUILD_SHARED=OFF; \
 dep libgme 0.6.5 game-music-emu 7079627509eeb30153f29167baf63d81ab1dfbe7 de346d4201a627393be8213e0a131dc647bbe8c778936d058640c1cb02d4d4a2 . \
     -DGME_BUILD_STATIC=ON -DGME_BUILD_SHARED=OFF -DGME_ZLIB=OFF; \
 dep wavpack 5.9.0 wavpack c88aaf8fb902c547a11053273df289d4b892335f 862b5dc729e67bb588eaf58cb748832fa6006f4d695e30757d22e0cbad8ecd59 . \
     -DWAVPACK_BUILD_PROGRAMS=OFF -DWAVPACK_INSTALL_DOCS=OFF -DWAVPACK_INSTALL_PKGCONFIG_MODULE=ON -DBUILD_TESTING=OFF; \
 install -Dm644 /opt/toolchains/deps/wavpack-5.9.0/src/include/wavpack.h /opt/toolchains/deps/wavpack-5.9.0/include/wavpack/wavpack.h; \
 dep mpg123 1.33.7 mpg123 0913b4aa7cefb3fb3f58d157b773f5905ba130c8 c0ffa8c276ecfa73286d02c970587351d7803ea72dd0e67833dc49811bd2c5e7 ports/cmake \
     -DBUILD_PROGRAMS=OFF -DBUILD_LIBOUT123=OFF; \
 dep fluidsynth 2.5.7 fluidsynth b53de2809bc5e4bffdaec59f447f81ba55ce4fe1 2b13cdab3ccb8ec4003c57dec95b561096398ecc39176e1990a3bb270204660f . \
     -Dosal=cpp11 -Denable-alsa=OFF -Denable-aufile=OFF -Denable-dbus=OFF -Denable-ipv6=OFF -Denable-jack=OFF \
     -Denable-ladspa=OFF -Denable-libinstpatch=OFF -Denable-libsndfile=OFF -Denable-midishare=OFF \
     -Denable-opensles=OFF -Denable-oboe=OFF -Denable-network=OFF -Denable-oss=OFF -Denable-sdl3=OFF \
     -Denable-pulseaudio=OFF -Denable-pipewire=OFF -Denable-readline=OFF -Denable-threads=OFF \
     -Denable-openmp=OFF -Denable-unicode=OFF -Denable-native=OFF -Denable-systemd=OFF; \
 chmod -R a+rX /opt/toolchains/deps
USER mayhem

# Source enters from the build CONTEXT (the CI checkout): the FULL repo (incl. .git
# history) is copied to /mayhem as `mayhem`; build.sh builds in place.
USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

# Anything a Dockerfile step materializes into the tree (downloaded fixtures, generated headers,
# vendored deps) MUST be committed or (re)created by build.sh: rlenv's patch grader runs
# `git clean -ffdX` before every graded build, so a gitignored, untracked file is gone by then and
# every honest patch scores zero (SPEC §6.2 item 16; verify-repo.sh checks it).
#
# OPTIONAL compile-cache pre-fill (SPEC §6.2 item 18). Bases minted with /opt/mayhem/ccache/prefill
# run the build under rlenv's pinned ccache, so the patch grader's first (otherwise cold) rebuild is
# warm — this is what keeps large C/C++ targets inside the build time limit. Older bases fall back to
# the plain build; a missing or stale pre-fill only costs a cold cache. Never enable ccache in
# build.sh or via ENV/PATH, and keep the compiler resolving by name (CC=clang) or nothing is cached.
RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

# Run the project's functional tests at build time IF the repo ships them (mayhem/test.sh is
# optional — omit it only when the repo has no tests meaningful for RL). A failing suite fails the
# image build, so the commit image is guaranteed to build AND pass its own tests.
RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
