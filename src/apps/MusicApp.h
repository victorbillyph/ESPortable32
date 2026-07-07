#ifndef APP_MUSIC_H
#define APP_MUSIC_H

#include "../core/AppManager.h"
#include "../core/Menu.h"
#include "../core/MediaBuffer.h"
#include "../core/Speaker.h"
#include "driver/i2s.h"

class MusicApp : public App {
public:
    MusicApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Musica"; }

private:
    enum Mode { MENU, LIST, BUILTIN, PLAYING };
    AppManager* _mgr;
    Menu _menu;
    Mode _mode;
    int _sel;
    const char* _items[4];
    const uint8_t* _icons[4];

    bool _playing;
    bool _paused;
    int _sampleRate;
    int _bitsPerSample;
    int _channels;
    uint32_t _dataOff;
    uint32_t _dataEnd;
    uint32_t _readPos;

    const uint8_t* _wavFlash;
    uint32_t _wavFlashSize;

    void drawList(Display& d);
    void drawBuiltin(Display& d);
    void drawPlaying(Display& d);
    void startPlay();
    void startWavFlash(const uint8_t* data, uint32_t size);
    void stopPlay();
    bool parseWav(const uint8_t* src, uint32_t srcSize);
    int fillBuffer(int16_t* buf, int frames);
    void installI2S();
    void uninstallI2S();
};

#endif
