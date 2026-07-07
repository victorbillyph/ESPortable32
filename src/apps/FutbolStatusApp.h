#ifndef APP_FUTBOL_H
#define APP_FUTBOL_H

#include "../core/AppManager.h"
#include "../core/Menu.h"
#include <time.h>

struct TeamInfo {
    const char* name;
    int apiId;
};

struct MatchData {
    String opponent;
    String homeScore;
    String awayScore;
    String status;
    String time;
    String competition;
    time_t matchEpoch;
    bool loaded;
};

class FutbolStatusApp : public App {
public:
    FutbolStatusApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Futebol"; }

private:
    enum Mode { MENU, TEAM_LIST, MATCH };
    AppManager* _mgr;
    Menu _menu;
    const char* _items[2];
    const uint8_t* _icons[2];
    Mode _mode;

    int _sel;
    int _teamIdx;
    unsigned long _lastFetch;
    unsigned long _lastRefresh;
    MatchData _match;
    bool _fetching;
    bool _fetchDone;

    bool _animActive;
    unsigned long _animStart;
    String _animGoalTeam;
    int _animAngle;

    static const int TEAM_COUNT = 24;
    static const TeamInfo TEAMS[TEAM_COUNT];

    void selectTeam(int idx);
    void fetchMatch();
    String httpGet(const String& url);
    void parseEvents(const String& data, int teamId);
    void drawTeams(Display& d);
    void drawMatch(Display& d);
    void drawGoalAnim(Display& d);
    time_t parseUtcEpoch(const String& ts);
};

#endif
