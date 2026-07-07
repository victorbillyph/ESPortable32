#include "Speaker.h"
#include "Config.h"

extern Config config;

int Speaker::_posPin = -1;
int Speaker::_negPin = -1;
bool Speaker::_init = false;

void Speaker::begin(int posPin, int negPin) {
    if (posPin < 0 || negPin < 0) {
        posPin = config.getInt("spk_pos", 25);
        negPin = config.getInt("spk_neg", 26);
    }
    _posPin = posPin;
    _negPin = negPin;

    if (_posPin < 0 || _negPin < 0) {
        _init = false;
        return;
    }

    pinMode(_negPin, OUTPUT);
    digitalWrite(_negPin, LOW);

    ledcSetup(SPEAKER_CHANNEL, 1000, SPEAKER_RES);
    ledcAttachPin(_posPin, SPEAKER_CHANNEL);
    ledcWrite(SPEAKER_CHANNEL, 0);

    _init = true;
}

void Speaker::setPins(int pos, int neg) {
    if (_init) {
        ledcDetachPin(_posPin);
        ledcWrite(SPEAKER_CHANNEL, 0);
    }
    _posPin = pos;
    _negPin = neg;
    _init = false;

    if (pos < 0 || neg < 0) return;

    pinMode(_negPin, OUTPUT);
    digitalWrite(_negPin, LOW);
    ledcSetup(SPEAKER_CHANNEL, 1000, SPEAKER_RES);
    ledcAttachPin(_posPin, SPEAKER_CHANNEL);
    ledcWrite(SPEAKER_CHANNEL, 0);
    _init = true;
}

int Speaker::getPosPin() { return _posPin; }
int Speaker::getNegPin() { return _negPin; }
bool Speaker::enabled() { return _init && _posPin >= 0 && _negPin >= 0; }

void Speaker::tone(int freq, int durationMs) {
    if (!enabled()) return;
    ledcWriteTone(SPEAKER_CHANNEL, freq);
    delay(durationMs);
    noTone();
}

void Speaker::noTone() {
    if (!_init) return;
    ledcWrite(SPEAKER_CHANNEL, 0);
}

void Speaker::toneStart(int freq) {
    if (!enabled()) return;
    if (freq > 0) ledcWriteTone(SPEAKER_CHANNEL, freq);
    else ledcWrite(SPEAKER_CHANNEL, 0);
}

void Speaker::toneStop() {
    if (!_init) return;
    ledcWrite(SPEAKER_CHANNEL, 0);
}

void Speaker::detach() {
    if (!_init) return;
    ledcDetachPin(_posPin);
}

void Speaker::playNotes(const int* freqs, const int* durations, int count) {
    for (int i = 0; i < count; i++) {
        tone(freqs[i], durations[i]);
        delay(30);
    }
}

void Speaker::playStartup() {
    int freqs[] = { 262, 330, 392, 523 };
    int durs[] = { 100, 100, 100, 200 };
    playNotes(freqs, durs, 4);
}

void Speaker::playBeep() {
    tone(1000, 80);
}

void Speaker::playError() {
    tone(200, 300);
}
