#include "audio_synth.h"
#include <math.h>
#include <stdlib.h>
#ifndef PI
#define PI 3.14159265358979323846f
#endif
typedef enum WaveForm { WF_SINE=0, WF_SQUARE, WF_SAW, WF_NOISE } WaveForm;
static Sound SynthTone(float fStart, float fEnd, float duration, WaveForm wf, float volume) {
    const unsigned int sr = 22050; unsigned int frames = (unsigned int)(sr * duration); if (frames < 64) frames = 64;
    short* pcm = (short*)RL_MALLOC(frames * sizeof(short));
    if (!pcm) { Sound e = {0}; return e; }
    float phase = 0.0f;
    for (unsigned int i = 0; i < frames; i++) {
        float t = (float)i / (float)frames, freq = fStart + (fEnd - fStart) * t; if (freq < 30.0f) freq = 30.0f;
        phase += 2.0f * PI * freq / (float)sr;
        float env = powf(1.0f - t, 1.55f), s = 0.0f;
        if (wf == WF_SINE) s = sinf(phase);
        else if (wf == WF_SQUARE) s = (sinf(phase) >= 0.0f) ? 1.0f : -1.0f;
        else if (wf == WF_SAW) s = 2.0f * ((phase / (2.0f * PI)) - floorf(phase / (2.0f * PI))) - 1.0f;
        else s = ((float)(rand() % 20001) / 10000.0f) - 1.0f;
        float out = s * env * volume * 32767.0f;
        if (out > 32767.0f) out = 32767.0f; if (out < -32767.0f) out = -32767.0f;
        pcm[i] = (short)out;
    }
    Wave w = {0}; w.frameCount = frames; w.sampleRate = sr; w.sampleSize = 16; w.channels = 1; w.data = pcm;
    Sound snd = LoadSoundFromWave(w); UnloadWave(w); return snd;
}
GameAudio Audio_Init(void) {
    GameAudio a = {0};
    a.sounds[SFX_BOUNCE] = SynthTone(240,130,0.05f,WF_SINE,0.18f); a.sounds[SFX_HIT] = SynthTone(190,70,0.09f,WF_SQUARE,0.22f);
    a.sounds[SFX_CRIT] = SynthTone(330,85,0.16f,WF_SAW,0.26f); a.sounds[SFX_BLADE] = SynthTone(640,410,0.06f,WF_SAW,0.16f);
    a.sounds[SFX_SHOOT] = SynthTone(540,210,0.06f,WF_SQUARE,0.14f); a.sounds[SFX_LASER] = SynthTone(920,260,0.16f,WF_SAW,0.20f);
    a.sounds[SFX_BOOM] = SynthTone(130,35,0.28f,WF_NOISE,0.30f); a.sounds[SFX_KO] = SynthTone(260,45,0.35f,WF_SAW,0.26f);
    a.sounds[SFX_CLICK] = SynthTone(520,680,0.04f,WF_SINE,0.15f); a.sounds[SFX_WIN] = SynthTone(440,880,0.32f,WF_SINE,0.26f);
    a.sounds[SFX_TIME] = SynthTone(160,45,0.45f,WF_SINE,0.32f); a.initialized = true; return a;
}
void Audio_Play(const GameAudio* a, SfxId id, bool on) { if (on && a && a->initialized && id >= 0 && id < SFX_COUNT) PlaySound(a->sounds[id]); }
void Audio_Unload(GameAudio* a) { if (!a || !a->initialized) return; for (int i = 0; i < SFX_COUNT; i++) UnloadSound(a->sounds[i]); a->initialized = false; }