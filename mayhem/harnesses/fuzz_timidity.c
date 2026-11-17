/* SDL_mixer's built-in timidity MIDI synthesizer, through the public decoder API with the
 * decoder pinned to "TIMIDITY". Three builds:
 *   -DMODE_MIDI  the input is a MIDI file; instruments come from a fixed tiny SoundFont.
 *   -DMODE_SF2   the input is the SoundFont (TIMIDITY_SOUNDFONT); a fixed tiny MIDI plays it.
 *   -DMODE_PAT   the input is a GUS .pat instrument, mapped by a fixed timidity.cfg
 *                (TIMIDITY_CFG) to a melodic program and a drum; a fixed tiny MIDI plays it.
 * Instruments are read by timidity only at MIX_Init() (Timidity_Init), so the SF2/PAT builds run
 * MIX_Init/MIX_Quit per input: nothing from one input survives to the next. Fixtures are written
 * once under $TMPDIR (default /tmp), never under the read-only image dir. The environment
 * variables are set before the first SDL call because SDL snapshots the environment. */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../fixtures/fixtures.h"

static char dir[512], input_path[600];
static int inited;

static int write_file(const char *path, const void *p, size_t n) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    size_t w = fwrite(p, 1, n, fp);
    return (fclose(fp) == 0) && w == n;
}

static void setup(void) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(dir, sizeof dir, "%s/sdlmix_timidity_%d", tmp, (int)getpid());
    mkdir(dir, 0700);
    char p[600];
#if defined(MODE_MIDI)
    snprintf(p, sizeof p, "%s/tiny.sf2", dir);
    if (!write_file(p, fixture_sf2, sizeof fixture_sf2)) abort();
    setenv("TIMIDITY_SOUNDFONT", p, 1);
#elif defined(MODE_SF2)
    snprintf(input_path, sizeof input_path, "%s/fuzz.sf2", dir);
    setenv("TIMIDITY_SOUNDFONT", input_path, 1);
#elif defined(MODE_PAT)
    snprintf(input_path, sizeof input_path, "%s/fuzz.pat", dir);
    snprintf(p, sizeof p, "%s/timidity.cfg", dir);
    char cfg[700];
    snprintf(cfg, sizeof cfg, "dir %s\nbank 0\n0 fuzz\ndrumset 0\n36 fuzz\n", dir);
    if (!write_file(p, cfg, strlen(cfg))) abort();
    setenv("TIMIDITY_CFG", p, 1);
#else
#error define MODE_MIDI, MODE_SF2 or MODE_PAT
#endif
}

static void decode(const uint8_t *data, size_t size) {
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, MIX_PROP_AUDIO_DECODER_STRING, "TIMIDITY");
    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (io) {
        MIX_AudioDecoder *d = MIX_CreateAudioDecoder_IO(io, false, props);
        if (d) {
            static char buf[16384];
            SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 22050 };
            for (int i = 0; i < 32; i++) {
                if (MIX_DecodeAudio(d, buf, sizeof buf, &spec) <= 0) break;
            }
            MIX_DestroyAudioDecoder(d);
        }
        SDL_CloseIO(io);
    }
    SDL_DestroyProperties(props);
}

/* SDL_assert is compiled in (build.sh: -DSDL_DEFAULT_ASSERT_LEVEL=2); make a failed one abort instead of
 * prompting, so it is a crash in the fuzzer and in the -standalone reproducer. The hint lives in SDL
 * core and is not reset by MIX_Quit. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!inited) {
        setup(); /* first: its setenv calls must precede SDL's environment snapshot */
        SDL_SetHint(SDL_HINT_ASSERT, "abort");
        inited = 1;
#if defined(MODE_MIDI)
        if (!MIX_Init()) abort();
#endif
    }
#if defined(MODE_MIDI)
    decode(data, size);
#else
    if (!write_file(input_path, data, size)) abort();
    if (!MIX_Init()) return 0;
    decode(fixture_mid, sizeof fixture_mid);
    MIX_Quit();
#endif
    return 0;
}
