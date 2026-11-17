/* Full mixer path: MIX_LoadAudio_IO (format auto-detect, optional predecode of the whole file),
 * a track with fuzzer-chosen play options (loops, start/max frame, fade-in, loop start) and
 * MIX_Generate, with an optional mid-play seek. The first 4 bytes pick the options; the rest is
 * the audio file. State kept across inputs: the mixer (format fixed); every track/audio made for
 * an input is destroyed before returning. */
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
    if (size < 4) return -1;
    uint8_t f = data[0], a = data[1], b = data[2], c = data[3];
    data += 4;
    size -= 4;
    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (!io) return 0;
    MIX_Audio *audio = MIX_LoadAudio_IO(mixer, io, (f & 1) != 0, false);
    if (audio) {
        MIX_Track *t = MIX_CreateTrack(mixer);
        if (t) {
            MIX_SetTrackAudio(t, audio);
            SDL_PropertiesID o = SDL_CreateProperties();
            if (f & 2) SDL_SetNumberProperty(o, MIX_PROP_PLAY_LOOPS_NUMBER, a & 3);
            if (f & 4) SDL_SetNumberProperty(o, MIX_PROP_PLAY_START_FRAME_NUMBER, b * 16);
            if (f & 8) SDL_SetNumberProperty(o, MIX_PROP_PLAY_MAX_FRAME_NUMBER, c * 16);
            if (f & 16) SDL_SetNumberProperty(o, MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER, a * 8);
            if (f & 32) SDL_SetNumberProperty(o, MIX_PROP_PLAY_LOOP_START_FRAME_NUMBER, b);
            MIX_PlayTrack(t, o);
            static float buf[4096];
            for (int i = 0; i < 32; i++) {
                if (MIX_Generate(mixer, buf, sizeof buf) <= 0) break;
                if (i == 8 && (f & 64)) MIX_SetTrackPlaybackPosition(t, c * 4);
            }
            SDL_DestroyProperties(o);
            MIX_DestroyTrack(t);
        }
        MIX_DestroyAudio(audio);
    }
    SDL_CloseIO(io);
    return 0;
}
