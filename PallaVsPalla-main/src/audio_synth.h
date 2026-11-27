#ifndef AUDIO_SYNTH_H
#define AUDIO_SYNTH_H
#include "raylib.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum SfxId { SFX_BOUNCE=0, SFX_HIT, SFX_CRIT, SFX_BLADE, SFX_SHOOT, SFX_LASER, SFX_BOOM, SFX_KO, SFX_CLICK, SFX_WIN, SFX_TIME, SFX_COUNT } SfxId;
typedef struct GameAudio { Sound sounds[SFX_COUNT]; bool initialized; } GameAudio;
GameAudio Audio_Init(void);
void Audio_Play(const GameAudio* audio, SfxId id, bool enabled);
void Audio_Unload(GameAudio* audio);
#ifdef __cplusplus
}
#endif
#endif