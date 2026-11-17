# Pulled in with -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES: make the decoder libraries' Find modules
# pick the static .a (the build links them statically; the sanitized binaries stay self-contained).
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
