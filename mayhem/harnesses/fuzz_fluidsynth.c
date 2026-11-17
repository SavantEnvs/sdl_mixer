/* (The soundfont property name is internal to SDL_mixer, so it is spelled out below.)
 * The FluidSynth MIDI decoder through the public decoder API, pinned to "FLUIDSYNTH". The input is
 * a MIDI file; the instruments come from the fixed tiny SoundFont, written once under $TMPDIR
 * (default /tmp) and passed by path. FluidSynth is built static without any audio driver. */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../fixtures/fixtures.h"

static char sf_path[600];
static int inited;

/* SDL_assert is compiled in (build.sh: -DSDL_DEFAULT_ASSERT_LEVEL=2); make a failed one abort instead of
 * prompting, so it is a crash in the fuzzer and in the -standalone reproducer. The hint lives in SDL
 * core and is not reset by MIX_Quit. */
static void setup(void) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    char dir[512];
    snprintf(dir, sizeof dir, "%s/sdlmix_fluid_%d", tmp, (int)getpid());
    mkdir(dir, 0700);
    snprintf(sf_path, sizeof sf_path, "%s/tiny.sf2", dir);
    FILE *fp = fopen(sf_path, "wb");
    if (!fp || fwrite(fixture_sf2, 1, sizeof fixture_sf2, fp) != sizeof fixture_sf2 || fclose(fp) != 0) abort();
    SDL_SetHint(SDL_HINT_ASSERT, "abort");
    if (!MIX_Init()) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!inited) {
        setup();
        inited = 1;
    }
    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (!io) return 0;
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, MIX_PROP_AUDIO_DECODER_STRING, "FLUIDSYNTH");
    SDL_SetStringProperty(props, "SDL_mixer.decoder.fluidsynth.soundfont_path", sf_path);
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
    SDL_DestroyProperties(props);
    return 0;
}
