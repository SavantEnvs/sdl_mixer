/* Port of the legacy mayhemheroes harness (fuzz/fuzz_load_music_from_mem.c), which called the
 * SDL2-era Mix_LoadMUS_IO/Mix_GetMusicType/Mix_FreeMusic. Upstream is now SDL3_mixer, whose
 * equivalent is MIX_LoadAudio_IO on a mixer (auto-detecting the format) plus the audio's
 * metadata/duration queries. The input is loaded from memory exactly like the old harness. */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdint.h>
#include <stdlib.h>

static MIX_Mixer *mixer;

/* SDL_assert is compiled in (build.sh: -DSDL_DEFAULT_ASSERT_LEVEL=2); make a failed one abort instead of
 * prompting, so it is a crash in the fuzzer and in the -standalone reproducer. The hint lives in SDL
 * core and is not reset by MIX_Quit. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!mixer) {
        SDL_SetHint(SDL_HINT_ASSERT, "abort");
        if (!MIX_Init()) abort();
        SDL_AudioSpec s = { SDL_AUDIO_F32, 2, 48000 };
        mixer = MIX_CreateMixer(&s);
        if (!mixer) abort();
    }
    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (!io) return 0;
    MIX_Audio *audio = MIX_LoadAudio_IO(mixer, io, false, false);
    if (audio) {
        SDL_AudioSpec spec;
        (void)MIX_GetAudioFormat(audio, &spec);
        (void)MIX_GetAudioDuration(audio);
        SDL_PropertiesID p = MIX_GetAudioProperties(audio);
        (void)SDL_GetStringProperty(p, MIX_PROP_METADATA_TITLE_STRING, NULL);
        MIX_DestroyAudio(audio);
    }
    SDL_CloseIO(io);
    return 0;
}
