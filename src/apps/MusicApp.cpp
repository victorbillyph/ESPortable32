#include "MusicApp.h"
#include "WavData.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"

static const char* const melodyNames[] = { "Mario Theme", "Rick Roll" };
static const int melodyCount = 2;

MusicApp::MusicApp(AppManager* mgr) : _mgr(mgr), _mode(MENU), _sel(0),
    _playing(false), _paused(false), _sampleRate(0), _bitsPerSample(0),
    _channels(0), _dataOff(0), _dataEnd(0), _readPos(0),
    _wavFlash(nullptr), _wavFlashSize(0) {}

void MusicApp::init() {
    _items[0] = "Listar Musicas";
    _items[1] = "Musicas de Exemplo";
    _items[2] = "Upload via Web";
    _items[3] = "Voltar";
    _icons[0] = ICON_FOLDER;
    _icons[1] = ICON_MUSIC;
    _icons[2] = ICON_WIFI;
    _icons[3] = ICON_BACK;
    _menu.setItems(_items, 4);
    _menu.setIcons(_icons);
    _menu.setTitle("Musica");
    _menu.setTitleIcon(ICON_INFO);
    _menu.reset();
    _mode = MENU;
    _playing = false;
    _paused = false;
}

void MusicApp::update() {
    if (_mode == PLAYING && _playing && !_paused) {
        int16_t buf[512];
        int frames = fillBuffer(buf, 256);
        if (frames == 0) { stopPlay(); _mode = _wavFlash ? BUILTIN : LIST; return; }

        uint32_t dac[256];
        for (int i = 0; i < frames; i++) {
            int16_t s = buf[i];
            uint16_t l = (uint16_t)((int32_t)s + 0x8000);
            uint16_t r = (uint16_t)((int32_t)(-s) + 0x8000);
            dac[i] = ((uint32_t)r << 16) | l;
        }
        size_t written;
        i2s_write(I2S_NUM_0, dac, frames * 4, &written, portMAX_DELAY);
    }
}

void MusicApp::draw(Display& d) {
    switch (_mode) {
        case MENU: _menu.draw(d); break;
        case LIST: drawList(d); break;
        case BUILTIN: drawBuiltin(d); break;
        case PLAYING: drawPlaying(d); break;
    }
}

void MusicApp::buttonClick() {
    if (_mode == PLAYING) _paused = !_paused;
    else if (_mode == MENU) _menu.next();
    else if (_mode == LIST) _sel = (_sel + 1) % 1;
    else if (_mode == BUILTIN) _sel = (_sel + 1) % melodyCount;
}

void MusicApp::buttonHold() {
    switch (_mode) {
        case MENU:
            switch (_menu.select()) {
                case 0: _mode = LIST; _sel = 0; break;
                case 1: _mode = BUILTIN; _sel = 0; break;
                case 2: _mode = LIST; break;
                case 3: _mgr->popApp(); break;
            }
            break;
        case LIST:
            if (mediaFile.valid) startPlay();
            break;
        case BUILTIN: {
            const uint8_t* wav = _sel == 0 ? wav_mario_theme : wav_rick_roll;
            uint32_t sz = _sel == 0 ? sizeof(wav_mario_theme) : sizeof(wav_rick_roll);
            startWavFlash(wav, sz);
            break;
        }
        case PLAYING: stopPlay(); break;
    }
}

void MusicApp::buttonVeryLong() { stopPlay(); _mode = MENU; }
void MusicApp::buttonDoubleClick() { stopPlay(); _mode = MENU; }
void MusicApp::exit() { stopPlay(); }

void MusicApp::installI2S() {
    Speaker::detach();
    int sp = Speaker::getPosPin();
    int sn = Speaker::getNegPin();
    if (sp >= 0) ledcDetachPin(sp);
    if (sn >= 0) ledcDetachPin(sn);
    i2s_driver_uninstall(I2S_NUM_0);
    delay(10);

    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate = (uint32_t)_sampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 128,
        .use_apll = 0,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT
#endif
    };

    esp_err_t e;
    e = i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    if (e != ESP_OK) { Serial.printf("[I2S] install: %d\n", e); return; }
    i2s_set_pin(I2S_NUM_0, NULL);
    e = i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    if (e != ESP_OK) Serial.printf("[I2S] dac_mode: %d\n", e);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void MusicApp::uninstallI2S() {
    _playing = false;
    i2s_driver_uninstall(I2S_NUM_0);
    Speaker::toneStop();
    _readPos = 0;
    _paused = false;
}

void MusicApp::startWavFlash(const uint8_t* data, uint32_t size) {
    _wavFlash = data;
    _wavFlashSize = size;
    if (!parseWav(data, size)) { _wavFlash = nullptr; return; }
    installI2S();
    _readPos = _dataOff;
    _paused = false;
    _playing = true;
    _mode = PLAYING;
}

void MusicApp::startPlay() {
    _wavFlash = nullptr;
    if (!parseWav(mediaData, mediaFile.size)) { Serial.println("[WAV] parse falhou"); return; }
    Serial.printf("[WAV] %dHz %dbit %dch\n", _sampleRate, _bitsPerSample, _channels);

    installI2S();
    _readPos = _dataOff;
    _paused = false;
    _playing = true;
    _mode = PLAYING;
}

void MusicApp::stopPlay() {
    _playing = false;
    i2s_driver_uninstall(I2S_NUM_0);
    Speaker::toneStop();
    _readPos = 0;
    _paused = false;
    _wavFlash = nullptr;
}

bool MusicApp::parseWav(const uint8_t* src, uint32_t srcSize) {
    if (!src || srcSize < 44) return false;
    if (memcmp(src, "RIFF", 4) != 0) return false;
    if (memcmp(src + 8, "WAVE", 4) != 0) return false;

    uint32_t pos = 12;
    while (pos + 8 <= srcSize) {
        uint32_t chunkLen = src[pos + 4] | (src[pos + 5] << 8) |
                            (src[pos + 6] << 16) | (src[pos + 7] << 24);
        if (memcmp(src + pos, "fmt ", 4) == 0) {
            if (chunkLen < 16) return false;
            uint16_t fmt = src[pos + 8] | (src[pos + 9] << 8);
            if (fmt != 1) return false;
            _channels = src[pos + 10] | (src[pos + 11] << 8);
            _sampleRate = src[pos + 12] | (src[pos + 13] << 8) |
                          (src[pos + 14] << 16) | (src[pos + 15] << 24);
            _bitsPerSample = src[pos + 22] | (src[pos + 23] << 8);
        } else if (memcmp(src + pos, "data", 4) == 0) {
            _dataOff = pos + 8;
            _dataEnd = _dataOff + chunkLen;
            if (_dataEnd > srcSize) _dataEnd = srcSize;
            return (_dataEnd > _dataOff);
        }
        pos += 8 + chunkLen;
        if (pos % 2) pos++;
    }
    return false;
}

int MusicApp::fillBuffer(int16_t* buf, int frames) {
    const uint8_t* src = _wavFlash ? _wavFlash : mediaData;
    int count = 0;
    while (count < frames && _readPos < _dataEnd) {
        int16_t s;
        if (_bitsPerSample == 8) {
            s = (((int16_t)src[_readPos]) - 128) << 8;
            _readPos++;
        } else {
            s = (int16_t)(src[_readPos] | (src[_readPos + 1] << 8));
            _readPos += 2;
        }
        if (_channels == 2) {
            if (_bitsPerSample == 8) _readPos++;
            else _readPos += 2;
        }
        buf[count++] = s;
    }
    return count;
}

void MusicApp::drawList(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Musicas", ICON_FOLDER);
    int y = 14;
    if (!mediaFile.valid) {
        d.drawCenteredText(28, "Nenhuma musica", 1);
        d.drawCenteredText(40, "Upload via web:", 1);
        d.drawCenteredText(50, "http://<IP>/upload", 1);
        d.show();
        return;
    }
    bool wav = (mediaFile.size >= 44 && memcmp(mediaData, "RIFF", 4) == 0);
    char buf[22];
    snprintf(buf, 21, "> %s", mediaFile.name);
    d.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, 21, "  %d bytes", (int)mediaFile.size);
    d.drawText(1, y, buf, 1); y += 10;
    d.drawText(1, y, wav ? "  WAV" : "  (formato invalido)", 1); y += 10;
    d.drawCenteredText(57, wav ? "Hold = tocar" : "", 1);
    d.show();
}

void MusicApp::drawBuiltin(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Musicas de Exemplo", ICON_MUSIC);
    for (int i = 0; i < melodyCount; i++) {
        char buf[22];
        snprintf(buf, 21, "%s %s", _sel == i ? ">" : " ", melodyNames[i]);
        d.drawText(1, 16 + i * 10, buf, 1);
    }
    d.drawCenteredText(57, "Hold = tocar", 1);
    d.show();
}

void MusicApp::drawPlaying(Display& d) {
    d.clear();
    d.drawCenteredText(2, "Tocando...", 1);
    d.drawHLine(0, 11, 128);
    char buf[22];
    if (_wavFlash) {
        snprintf(buf, 21, "%s", melodyNames[_sel]);
    } else {
        snprintf(buf, 21, "%s", mediaFile.valid ? mediaFile.name : "");
    }
    d.drawCenteredText(14, buf, 1);
    int total = _dataEnd - _dataOff;
    int cur = _readPos - _dataOff;
    int pct = (cur * 100) / max(1, total);
    d.drawProgressBar(14, 28, 100, 8, pct);
    snprintf(buf, 21, "%d%%", pct);
    d.drawCenteredText(40, buf, 1);
    if (_paused) d.drawCenteredText(50, "PAUSADO", 1);
    d.drawCenteredText(57, "Hold=parar", 1);
    d.show();
}
