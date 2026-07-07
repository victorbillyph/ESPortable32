#ifndef CORE_SPEAKER_H
#define CORE_SPEAKER_H

#include <Arduino.h>

#define SPEAKER_CHANNEL 0
#define SPEAKER_RES 10

class Speaker {
public:
    static void begin(int posPin = -1, int negPin = -1);
    static void setPins(int pos, int neg);
    static int getPosPin();
    static int getNegPin();
    static bool enabled();

    static void tone(int freq, int durationMs);
    static void noTone();
    static void toneStart(int freq);
    static void toneStop();
    static void detach();

    static void playNotes(const int* freqs, const int* durations, int count);
    static void playStartup();
    static void playBeep();
    static void playError();

private:
    static int _posPin;
    static int _negPin;
    static bool _init;
};

#endif
