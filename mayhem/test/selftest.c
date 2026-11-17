/* Known-answer decode check for mayhem/test.sh: for each "<DECODER> <file>" pair, decode the whole
 * file as S16 stereo 22050 Hz through MIX_CreateAudioDecoder_IO/MIX_DecodeAudio and print the
 * decoded byte count and an FNV-1a hash of the PCM. test.sh diffs this against golden values.
 * Decoding stops after 4 MiB of PCM: a chiptune (GME) has no length and would play forever. */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv) {
    if (!MIX_Init()) { fprintf(stderr, "MIX_Init: %s\n", SDL_GetError()); return 2; }
    int rc = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetStringProperty(props, MIX_PROP_AUDIO_DECODER_STRING, argv[i]);
        MIX_AudioDecoder *d = MIX_CreateAudioDecoder(argv[i + 1], props);
        if (!d) {
            printf("%s %s FAIL %s\n", argv[i], argv[i + 1], SDL_GetError());
            rc = 1;
        } else {
            static unsigned char buf[8192];
            SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 22050 };
            uint64_t total = 0, h = 1469598103934665603ULL;
            int n;
            while (total < (4u << 20) && (n = MIX_DecodeAudio(d, buf, sizeof buf, &spec)) > 0) {
                for (int k = 0; k < n; k++) { h ^= buf[k]; h *= 1099511628211ULL; }
                total += (uint64_t)n;
            }
            printf("%s %s bytes=%llu fnv=%016llx\n", argv[i], argv[i + 1],
                   (unsigned long long)total, (unsigned long long)h);
            MIX_DestroyAudioDecoder(d);
        }
        SDL_DestroyProperties(props);
    }
    MIX_Quit();
    return rc;
}
