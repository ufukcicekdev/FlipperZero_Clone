#ifndef SOUND_H
#define SOUND_H

enum SystemSound {
    SOUND_SCROLL,
    SOUND_CLICK,
    SOUND_BACK,
    SOUND_ENTER
};

void initAudio();
void playTone(int freq, int durationMs);
void playSystemSound(SystemSound snd);

#endif