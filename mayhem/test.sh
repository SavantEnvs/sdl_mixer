#!/usr/bin/env bash
#
# mayhem/test.sh — known-answer decode check (SDL_mixer's own test/ programs open a real audio
# device and assert nothing, so there is no upstream suite to run). build.sh built
# build-tests/selftest with normal flags; for each fixture it decodes the whole file through the
# public decoder API and must reproduce the golden byte count + FNV-1a hash of the PCM in
# mayhem/test/expected.txt. One CTRF test per file.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
SRC="${SRC:-/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BIN=build-tests/selftest
if [ ! -x "$BIN" ]; then
  echo "test.sh: $BIN missing — mayhem/build.sh must build it" >&2
  emit_ctrf "sdl-mixer-selftest" 0 1
  exit 1
fi

# The MIDI case needs an instrument set; point timidity at the committed tiny SoundFont.
export TIMIDITY_SOUNDFONT="$SRC/mayhem/fixtures/tiny.sf2"
export SDL_AUDIODRIVER=dummy

passed=0 failed=0
while read -r dec file want_bytes want_fnv; do
  [ -z "$dec" ] && continue
  got=$("$BIN" "$dec" "$file" 2>/dev/null | head -1)
  want="$dec $file $want_bytes $want_fnv"
  if [ "$got" = "$want" ]; then
    passed=$((passed + 1)); echo "PASS $dec $file"
  else
    failed=$((failed + 1)); echo "FAIL $dec $file: got '$got' want '$want'"
  fi
done < mayhem/test/expected.txt

emit_ctrf "sdl-mixer-selftest" "$passed" "$failed"
