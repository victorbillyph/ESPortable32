#ifndef APP_GAMES_H
#define APP_GAMES_H

#include "../core/AppManager.h"
#include "../core/Menu.h"

class GamesApp : public App {
public:
    GamesApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Games"; }

private:
    enum GameView { GAME_MENU, GAME_SNAKE, GAME_RITMO };
    AppManager* _mgr;
    Menu _menu;
    GameView _view;
    const char* _items[3];
    const uint8_t* _icons[3];

    // Snake state
    struct Snake {
        int dir;
        int body[200];
        int len;
        int foodX, foodY;
        int score;
        int highScore;
        bool alive;
    } _snake;
    unsigned long _snakeTimer;

    void snakeInit();
    void snakeUpdate();
    void snakeDraw(Display& d);

    // Ritmo state
    struct Note {
        float y;
        bool active;
    };
    struct Ritmo {
        Note notes[8];
        int score;
        int combo;
        int maxCombo;
        int totalNotes;
        int hitNotes;
        float speed;
        unsigned long lastNote;
        unsigned long lastFrame;
        bool active;
        int streak;
    } _ritmo;

    void ritmoInit();
    void ritmoUpdate();
    void ritmoDraw(Display& d);
    void ritmoSpawnNote();
};

#endif
