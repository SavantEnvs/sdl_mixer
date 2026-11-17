/* Decode one audio file through SDL_mixer's public decoder API:
 * MIX_CreateAudioDecoder_IO + MIX_DecodeAudio. Built once per in-tree decoder with
 * -DDECODER_NAME="WAV" (etc.), which pins MIX_PROP_AUDIO_DECODER_STRING so the fuzzer's effort
 * is not split across formats; built without it, the library auto-detects the format.
 * State kept across inputs: only MIX_Init() (decoder registry, read-only after init). */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdint.h>
#include <stdlib.h>

static int inited;

/* SDL_assert is compiled in (build.sh: -DSDL_DEFAULT_ASSERT_LEVEL=2); make a failed one abort instead of
 * prompting, so it is a crash in the fuzzer and in the -standalone reproducer. The hint lives in SDL
 * core and is not reset by MIX_Quit. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!inited) {
        SDL_SetHint(SDL_HINT_ASSERT, "abort");
        if (!MIX_Init()) abort();
        inited = 1;
    }
    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (!io) return 0;
    SDL_PropertiesID props = 0;
#ifdef DECODER_NAME
    props = SDL_CreateProperties();
    SDL_SetStringProperty(props, MIX_PROP_AUDIO_DECODER_STRING, DECODER_NAME);
#endif
    /* closeio=false: on a failed load the library does not close io itself, so we always do. */
    MIX_AudioDecoder *d = MIX_CreateAudioDecoder_IO(io, false, props);
    if (d) {
        static char buf[16384];
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
        for (int i = 0; i < 64; i++) {
            if (MIX_DecodeAudio(d, buf, sizeof buf, &spec) <= 0) break;
        }
        MIX_DestroyAudioDecoder(d);
    }
    SDL_CloseIO(io);
    if (props) SDL_DestroyProperties(props);
    return 0;
}
