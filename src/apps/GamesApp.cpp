#include "GamesApp.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"
#include <stdlib.h>

GamesApp::GamesApp(AppManager* mgr) : _mgr(mgr), _view(GAME_MENU), _snakeTimer(0) {}

void GamesApp::init() {
    _items[0] = "Cobrinha";
    _items[1] = "Ritimo";
    _items[2] = "Voltar";
    _icons[0] = ICON_GAME;
    _icons[1] = ICON_GAME;
    _icons[2] = ICON_BACK;
    _menu.setItems(_items, 3);
    _menu.setIcons(_icons);
    _menu.setTitle("Jogos");
    _menu.setTitleIcon(ICON_GAME);
    _menu.reset();
    _view = GAME_MENU;
}

void GamesApp::update() {
    if (_view == GAME_SNAKE && _snake.alive) {
        if (millis() - _snakeTimer > 200) {
            _snakeTimer = millis();
            snakeUpdate();
        }
    }
    if (_view == GAME_RITMO && _ritmo.active) {
        if (millis() - _ritmo.lastFrame > 30) {
            _ritmo.lastFrame = millis();
            ritmoUpdate();
        }
    }
}

void GamesApp::draw(Display& d) {
    switch (_view) {
        case GAME_MENU: _menu.draw(d); break;
        case GAME_SNAKE: snakeDraw(d); break;
        case GAME_RITMO: ritmoDraw(d); break;
    }
}

void GamesApp::buttonClick() {
    switch (_view) {
        case GAME_MENU: _menu.next(); break;
        case GAME_SNAKE:
            _snake.dir = (_snake.dir + 1) % 4;
            break;
        case GAME_RITMO:
            if (_ritmo.active) {
                int hitIdx = -1;
                int bestDist = 99;
                for (int i = 0; i < 8; i++) {
                    if (!_ritmo.notes[i].active) continue;
                    int dist = abs((int)_ritmo.notes[i].y - 52);
                    if (dist < bestDist) {
                        bestDist = dist;
                        hitIdx = i;
                    }
                }
                if (hitIdx >= 0 && bestDist <= 10) {
                    _ritmo.notes[hitIdx].active = false;
                    _ritmo.totalNotes++;
                    _ritmo.hitNotes++;
                    _ritmo.combo++;
                    if (_ritmo.combo > _ritmo.maxCombo) _ritmo.maxCombo = _ritmo.combo;
                    _ritmo.streak = _ritmo.combo;
                    _ritmo.score += (bestDist <= 3) ? 100 + _ritmo.combo * 5 : 50 + _ritmo.combo * 2;
                } else {
                    _ritmo.combo = 0;
                    _ritmo.streak = -1;
                }
            }
            break;
    }
}

void GamesApp::buttonHold() {
    switch (_view) {
        case GAME_MENU:
            switch (_menu.select()) {
                case 0: snakeInit(); _view = GAME_SNAKE; break;
                case 1: ritmoInit(); _view = GAME_RITMO; break;
                case 2: _mgr->popApp(); break;
            }
            break;
        case GAME_SNAKE:
            if (!_snake.alive) snakeInit();
            else _view = GAME_MENU;
            break;
        case GAME_RITMO:
            _view = GAME_MENU;
            break;
    }
}

void GamesApp::buttonVeryLong() { _view = GAME_MENU; }
void GamesApp::buttonDoubleClick() {}
void GamesApp::exit() {}

// ── Cobrinha ──────────────────────────────────────

#define SNAKE_W (SCREEN_W / 4)
#define SNAKE_H (SCREEN_H / 4)
#define SNAKE_CELL 4

void GamesApp::snakeInit() {
    _snake.len = 4;
    _snake.dir = 1;
    _snake.score = 0;
    _snake.alive = true;
    for (int i = 0; i < _snake.len; i++) {
        _snake.body[i] = (5 - i) * 100 + 5;
    }
    _snake.foodX = rand() % SNAKE_W;
    _snake.foodY = rand() % SNAKE_H;
    _snakeTimer = millis();
}

void GamesApp::snakeUpdate() {
    if (!_snake.alive) return;
    int head = _snake.body[0];
    int hx = head / 100;
    int hy = head % 100;
    switch (_snake.dir) {
        case 0: hy--; break;
        case 1: hx++; break;
        case 2: hy++; break;
        case 3: hx--; break;
    }
    if (hx < 0) hx = SNAKE_W - 1;
    if (hx >= SNAKE_W) hx = 0;
    if (hy < 0) hy = SNAKE_H - 1;
    if (hy >= SNAKE_H) hy = 0;
    for (int i = 0; i < _snake.len; i++) {
        if (_snake.body[i] == hx * 100 + hy) { _snake.alive = false; return; }
    }
    for (int i = _snake.len - 1; i > 0; i--) {
        _snake.body[i] = _snake.body[i - 1];
    }
    _snake.body[0] = hx * 100 + hy;
    if (hx == _snake.foodX && hy == _snake.foodY) {
        _snake.len++;
        if (_snake.len > 199) _snake.len = 199;
        _snake.body[_snake.len - 1] = _snake.body[_snake.len - 2];
        _snake.score += 10;
        if (_snake.score > _snake.highScore) _snake.highScore = _snake.score;
        _snake.foodX = rand() % SNAKE_W;
        _snake.foodY = rand() % SNAKE_H;
    }
}

void GamesApp::snakeDraw(Display& d) {
    d.clear();
    char buf[20];
    snprintf(buf, sizeof(buf), "%d  HI:%d", _snake.score, _snake.highScore);
    d.drawCenteredText(0, buf, 1);
    d.drawHLine(0, 8, SCREEN_W);
    if (!_snake.alive) {
        d.drawCenteredText(28, "GAME OVER", 2);
        d.drawCenteredText(50, "Segure p/ jogar", 1);
        d.show();
        return;
    }
    for (int i = 0; i < _snake.len; i++) {
        int x = _snake.body[i] / 100;
        int y = _snake.body[i] % 100;
        d.oled().fillRect(x * SNAKE_CELL, y * SNAKE_CELL + 10, SNAKE_CELL - 1, SNAKE_CELL - 1, SSD1306_WHITE);
    }
    d.oled().fillCircle(_snake.foodX * SNAKE_CELL + 1, _snake.foodY * SNAKE_CELL + 11, 1, SSD1306_WHITE);
    d.show();
}

// ── Ritimo ────────────────────────────────────────

void GamesApp::ritmoInit() {
    _ritmo.score = 0;
    _ritmo.combo = 0;
    _ritmo.maxCombo = 0;
    _ritmo.totalNotes = 0;
    _ritmo.hitNotes = 0;
    _ritmo.speed = 1.0;
    _ritmo.lastNote = 0;
    _ritmo.lastFrame = 0;
    _ritmo.active = true;
    _ritmo.streak = 0;
    for (int i = 0; i < 8; i++) {
        _ritmo.notes[i].y = 0;
        _ritmo.notes[i].active = false;
    }
}

void GamesApp::ritmoSpawnNote() {
    for (int i = 0; i < 8; i++) {
        if (!_ritmo.notes[i].active) {
            _ritmo.notes[i].y = 0;
            _ritmo.notes[i].active = true;
            return;
        }
    }
}

void GamesApp::ritmoUpdate() {
    int interval = 600 - _ritmo.speed * 50;
    if (interval < 200) interval = 200;
    if (millis() - _ritmo.lastNote > (unsigned long)interval) {
        _ritmo.lastNote = millis();
        ritmoSpawnNote();
    }

    for (int i = 0; i < 8; i++) {
        if (!_ritmo.notes[i].active) continue;
        _ritmo.notes[i].y += _ritmo.speed;
        if (_ritmo.notes[i].y > 64) {
            _ritmo.notes[i].active = false;
            _ritmo.combo = 0;
            _ritmo.streak = -1;
        }
    }

    _ritmo.speed += 0.002;
    if (_ritmo.speed > 3.5) _ritmo.speed = 3.5;
}

void GamesApp::ritmoDraw(Display& d) {
    d.clear();

    d.oled().drawFastVLine(59, 10, 44, SSD1306_WHITE);
    d.oled().drawFastVLine(68, 10, 44, SSD1306_WHITE);
    d.drawHLine(58, 52, 12);

    d.oled().drawRect(57, 10, 14, 44, SSD1306_WHITE);

    for (int i = 0; i < 8; i++) {
        if (!_ritmo.notes[i].active) continue;
        int ny = (int)_ritmo.notes[i].y + 10;
        if (ny > 8 && ny < 56) {
            int r = 4;
            int cx = 64;
            d.oled().fillCircle(cx, ny, r, SSD1306_WHITE);
        }
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", _ritmo.score);
    d.drawText(0, 0, buf, 1);

    if (_ritmo.combo > 1) {
        snprintf(buf, sizeof(buf), "x%d", _ritmo.combo);
        int16_t x1, y1; uint16_t w, h;
        d.oled().getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
        d.drawText(SCREEN_W - w - 1, 0, buf, 1);
    }

    if (_ritmo.streak == -1) {
        d.drawCenteredText(57, "MISS", 1);
    }

    d.show();
}
