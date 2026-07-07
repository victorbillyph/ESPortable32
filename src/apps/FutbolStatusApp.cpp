#include "FutbolStatusApp.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"
#include "../core/State.h"
#include <HTTPClient.h>
#include <WiFi.h>

const TeamInfo FutbolStatusApp::TEAMS[FutbolStatusApp::TEAM_COUNT] = {
    { "Brasil", 134496 },
    { "Argentina", 134509 },
    { "Alemanha", 133907 },
    { "Franca", 133913 },
    { "Portugal", 133908 },
    { "Espanha", 133909 },
    { "Inglaterra", 133914 },
    { "Holanda", 133905 },
    { "Italia", 133910 },
    { "Uruguai", 134504 },
    { "Belgica", 134515 },
    { "Croacia", 133912 },
    { "Suica", 134506 },
    { "Mexico", 134497 },
    { "Japao", 134503 },
    { "Coreia do Sul", 134517 },
    { "EUA", 134514 },
    { "Colombia", 134501 },
    { "Chile", 134499 },
    { "Nigeria", 134512 },
    { "Senegal", 136143 },
    { "Gana", 134513 },
    { "Camaroes", 134498 },
    { "Marrocos", 136139 },
};

FutbolStatusApp::FutbolStatusApp(AppManager* mgr) : _mgr(mgr),
    _mode(MENU), _sel(0), _teamIdx(-1), _lastFetch(0), _lastRefresh(0),
    _fetching(false), _fetchDone(false),
    _animActive(false), _animStart(0), _animAngle(0) {
    _match.loaded = false;
    _match.matchEpoch = 0;
}

void FutbolStatusApp::init() {
    _items[0] = "Selecionar Time";
    _items[1] = "Voltar";
    _icons[0] = ICON_GAME;
    _icons[1] = ICON_BACK;
    _menu.setItems(_items, 2);
    _menu.setIcons(_icons);
    _menu.setTitle("Futebol");
    _menu.setTitleIcon(ICON_GAME);
    _menu.reset();
    _mode = MENU;
    _match.loaded = false;
    _animActive = false;
}

void FutbolStatusApp::update() {
    if (_animActive && millis() - _animStart > 3000) {
        _animActive = false;
    }

    if (_mode == MATCH) {
        if (_fetching && _fetchDone) {
            _fetching = false;
        }
        if (!_fetching && !_animActive && _match.loaded && millis() - _lastRefresh > 3000) {
            _fetching = true;
            _fetchDone = false;
            _match.loaded = false;
            _lastRefresh = millis();
        }
    }
}

void FutbolStatusApp::draw(Display& d) {
    switch (_mode) {
        case MENU: _menu.draw(d); break;
        case TEAM_LIST: drawTeams(d); break;
        case MATCH: drawMatch(d); break;
    }
}

void FutbolStatusApp::buttonClick() {
    switch (_mode) {
        case MENU: _menu.next(); break;
        case TEAM_LIST:
            if (TEAM_COUNT > 0) _sel = (_sel + 1) % TEAM_COUNT;
            break;
        default: break;
    }
}

void FutbolStatusApp::buttonHold() {
    switch (_mode) {
        case MENU:
            switch (_menu.select()) {
                case 0: _mode = TEAM_LIST; _sel = 0; break;
                case 1: _mgr->popApp(); break;
            }
            break;
        case TEAM_LIST:
            selectTeam(_sel);
            break;
        case MATCH: _mode = TEAM_LIST; break;
    }
}

void FutbolStatusApp::buttonVeryLong() { _mode = MENU; }
void FutbolStatusApp::buttonDoubleClick() { _mode = MENU; }
void FutbolStatusApp::exit() {}

void FutbolStatusApp::selectTeam(int idx) {
    _teamIdx = idx;
    _match.loaded = false;
    _fetching = true;
    _fetchDone = false;
    _animActive = false;
    _mode = MATCH;
    _lastFetch = 0;
    _lastRefresh = millis();
}

String FutbolStatusApp::httpGet(const String& url) {
    if (!state.wifiConnected) return "";
    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    int code = http.GET();
    if (code == 200) {
        String r = http.getString();
        http.end();
        return r;
    }
    http.end();
    return "";
}

static String extractJsonStr(const String& data, const String& key, int start) {
    String k = "\"" + key + "\":\"";
    int pos = data.indexOf(k, start);
    if (pos < 0) return "";
    pos += k.length();
    int end = data.indexOf('"', pos);
    if (end < 0) return "";
    return data.substring(pos, end);
}

static String extractJsonInt(const String& data, const String& key, int start) {
    String k = "\"" + key + "\":";
    int pos = data.indexOf(k, start);
    if (pos < 0) return "";
    pos += k.length();
    if (pos >= (int)data.length()) return "";
    bool quoted = (data[pos] == '"');
    if (quoted) pos++;
    String num;
    while (pos < (int)data.length() && data[pos] != ',' && data[pos] != '}' && data[pos] != '"') {
        num += data[pos++];
    }
    num.trim();
    return num;
}

time_t FutbolStatusApp::parseUtcEpoch(const String& ts) {
    if (ts.length() < 19) return 0;
    int y = ts.substring(0, 4).toInt();
    int mo = ts.substring(5, 7).toInt();
    int d = ts.substring(8, 10).toInt();
    int h = ts.substring(11, 13).toInt();
    int mi = ts.substring(14, 16).toInt();
    int s = ts.substring(17, 19).toInt();

    int mm = (mo - 3 + 12) % 12;
    int yy = (mm >= 10) ? y - 1 : y;
    int era = yy / 400;
    int yoe = yy - era * 400;
    int doy = (153 * mm + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int days = era * 146097 + doe - 719468;
    return (time_t)days * 86400 + h * 3600 + mi * 60 + s;
}

void FutbolStatusApp::fetchMatch() {
    if (_teamIdx < 0 || _teamIdx >= TEAM_COUNT) {
        _match.loaded = false;
        _fetchDone = true;
        return;
    }

    int teamId = TEAMS[_teamIdx].apiId;
    String name = TEAMS[_teamIdx].name;

    String url = "https://www.thesportsdb.com/api/v1/json/3/eventsnext.php?id=" + String(teamId);
    String data = httpGet(url);

    if (data.length() > 0 && data.indexOf("\"events\"") >= 0) {
        parseEvents(data, teamId);
        if (_match.loaded) { _fetchDone = true; return; }
    }

    url = "https://www.thesportsdb.com/api/v1/json/3/eventslast.php?id=" + String(teamId);
    data = httpGet(url);
    if (data.length() > 0) {
        parseEvents(data, teamId);
    }

    _fetchDone = true;
}

void FutbolStatusApp::parseEvents(const String& data, int teamId) {
    if (_teamIdx < 0 || _teamIdx >= TEAM_COUNT) return;
    String teamName = TEAMS[_teamIdx].name;

    int arrPos = data.indexOf("\"events\"");
    if (arrPos < 0) return;
    arrPos = data.indexOf('[', arrPos);
    if (arrPos < 0) return;

    int objPos = data.indexOf('{', arrPos);
    if (objPos < 0) return;

    String homeTeam = extractJsonStr(data, "strHomeTeam", objPos);
    String awayTeam = extractJsonStr(data, "strAwayTeam", objPos);
    String homeScore = extractJsonInt(data, "intHomeScore", objPos);
    String awayScore = extractJsonInt(data, "intAwayScore", objPos);
    String status = extractJsonStr(data, "strStatus", objPos);
    String matchTime = extractJsonStr(data, "strTime", objPos);
    String league = extractJsonStr(data, "strLeague", objPos);
    String timestamp = extractJsonStr(data, "strTimestamp", objPos);
    String idHome = extractJsonStr(data, "idHomeTeam", objPos);

    if (homeTeam.length() == 0) return;

    bool isHome = false;
    if (idHome.length() > 0 && idHome == String(teamId)) {
        isHome = true;
    } else {
        isHome = homeTeam.equalsIgnoreCase(teamName);
    }

    bool hadScore = _match.loaded && _match.homeScore.length() > 0;

    _match.opponent = isHome ? awayTeam : homeTeam;
    _match.competition = league;
    _match.time = "";

    bool hasScore = (homeScore.length() > 0) || (awayScore.length() > 0);
    bool hasLiveStatus = (status == "LIVE" || status == "IN PLAY" || status == "AO VIVO" ||
                          status == "HT" || status == "1H" || status == "2H" ||
                          status == "ET" || status == "PEN");

    if (hasLiveStatus) {
        _match.status = "Ao Vivo";
        if (status == "HT") _match.time = "Intervalo";
        else if (status == "1H") _match.time = "1° Tempo";
        else if (status == "2H") _match.time = "2° Tempo";
        else if (status == "ET") _match.time = "Prorrogacao";
        else if (status == "PEN") _match.time = "Penaltis";
        else _match.time = matchTime.length() > 0 ? matchTime : "-";
    } else if (status == "FT" || status == "FINAL" || status == "FINALIZADO" ||
               status == "FINISHED" || status == "ENDED") {
        _match.status = "Finalizado";
    } else if (status == "NS" || status == "NOT STARTED" || status == "SCHEDULED" ||
               status == "TIMED") {
        _match.status = "Agendado";
    } else if (status == "CANCELED" || status == "POSTPONED") {
        _match.status = "Cancelado";
    }

    if (_match.status.length() == 0) {
        if (homeScore.length() == 0 && awayScore.length() == 0) {
            _match.status = "Agendado";
        } else {
            _match.status = hasScore ? "Ao Vivo" : "Finalizado";
        }
    }

    if (_match.status == "Ao Vivo" && hadScore) {
        if (_match.homeScore != homeScore || _match.awayScore != awayScore) {
            String scorer = (_match.homeScore != homeScore) ? homeTeam : awayTeam;
            _animGoalTeam = scorer;
            _animActive = true;
            _animStart = millis();
            _animAngle = 0;
        }
    }

    _match.homeScore = isHome ? homeScore : awayScore;
    _match.awayScore = isHome ? awayScore : homeScore;
    _match.matchEpoch = 0;

    if (_match.status == "Agendado" && timestamp.length() >= 19) {
        _match.matchEpoch = parseUtcEpoch(timestamp);
        char buf[7];
        int hh = (timestamp.substring(11, 13).toInt() - 3 + 24) % 24;
        snprintf(buf, sizeof(buf), "%02d:%02d", hh, timestamp.substring(14, 16).toInt());
        _match.time = timestamp.substring(0, 10) + " " + String(buf);
    } else if (_match.status != "Ao Vivo") {
        _match.time = matchTime;
    }

    _match.loaded = true;
}

void FutbolStatusApp::drawTeams(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Times", ICON_GAME);

    int y = 13;
    int maxShow = 5;
    int s = _sel < maxShow ? 0 : _sel - maxShow + 1;
    int e = min(s + maxShow, TEAM_COUNT);

    for (int i = s; i < e; i++) {
        char buf[22];
        char prefix = (i == _sel) ? '>' : ' ';
        snprintf(buf, sizeof(buf), "%c%s", prefix, TEAMS[i].name);
        d.drawText(1, y, buf, 1);
        y += 10;
    }

    d.drawCenteredText(57, "Hold=selecionar", 1);
    d.show();
}

void FutbolStatusApp::drawGoalAnim(Display& d) {
    d.clear();

    _animAngle = ((millis() - _animStart) / 50) % 360;
    float rad = _animAngle * 3.14159 / 180.0;

    int cx = 64, cy = 28, r = 12;
    d.oled().drawCircle(cx, cy, r, SSD1306_WHITE);
    d.oled().drawCircle(cx, cy, 1, SSD1306_WHITE);

    int x1 = cx + cos(rad) * r;
    int y1 = cy + sin(rad) * r;
    int x2 = cx - cos(rad) * r;
    int y2 = cy - sin(rad) * r;
    d.oled().drawLine(x1, y1, x2, y2, SSD1306_WHITE);

    rad += 1.5708;
    x1 = cx + cos(rad) * (r - 2);
    y1 = cy + sin(rad) * (r - 2);
    x2 = cx - cos(rad) * (r - 2);
    y2 = cy - sin(rad) * (r - 2);
    d.oled().drawLine(x1, y1, x2, y2, SSD1306_WHITE);

    d.oled().setTextSize(2);
    d.oled().setTextColor(SSD1306_WHITE);
    d.drawCenteredText(44, "GOL!", 2);

    d.oled().setTextSize(1);
    String goalTeam = _animGoalTeam;
    if (goalTeam.length() > 16) goalTeam = goalTeam.substring(0, 15) + "~";
    d.drawCenteredText(57, goalTeam.c_str(), 1);

    d.show();
}

void FutbolStatusApp::drawMatch(Display& d) {
    if (_animActive) {
        drawGoalAnim(d);
        return;
    }

    d.clear();

    if (_fetching && !_fetchDone) {
        fetchMatch();
    }

    if (!_match.loaded) {
        GUI::drawMenuTitle(d, TEAMS[_teamIdx].name, ICON_GAME);
        if (!state.wifiConnected) {
            d.drawCenteredText(28, "WiFi desligado", 1);
        } else {
            d.drawCenteredText(24, "Buscando...", 1);
            int dots = (millis() / 400) % 4;
            char buf[22];
            snprintf(buf, sizeof(buf), "jogos%s", String("...").substring(0, dots).c_str());
            d.drawCenteredText(34, buf, 1);
        }
        d.drawCenteredText(54, "Hold=voltar", 1);
        d.show();
        return;
    }

    int y = 2;
    char buf[24];

    snprintf(buf, sizeof(buf), "%s vs %s", TEAMS[_teamIdx].name, _match.opponent.c_str());
    if (strlen(buf) > 20) buf[20] = '\0';
    d.drawCenteredText(y, buf, 1); y += 11;

    if (_match.status == "Agendado") {
        d.oled().setTextSize(2);
        d.oled().setTextColor(SSD1306_WHITE);
        int16_t x1, y1; uint16_t w, h;
        d.oled().getTextBounds("Agendado", 0, 0, &x1, &y1, &w, &h);
        d.oled().setCursor((128 - w) / 2, y);
        d.oled().print("Agendado");
        y += 18;
        d.oled().setTextSize(1);

        time_t now_t = time(nullptr);
        time_t match_t = _match.matchEpoch;
        if (match_t > 0) {
            double diff = difftime(match_t, now_t);
            if (diff > 0) {
                int days = diff / 86400;
                int hours = ((int)diff % 86400) / 3600;
                int mins = ((int)diff % 3600) / 60;
                int secs = (int)diff % 60;
                char cd[22];
                if (days > 0) {
                    snprintf(cd, sizeof(cd), "%dd %02dh %02dm", days, hours, mins);
                } else if (hours > 0) {
                    snprintf(cd, sizeof(cd), "%02dh %02dm %02ds", hours, mins, secs);
                } else {
                    snprintf(cd, sizeof(cd), "%02dm %02ds", mins, secs);
                }
                d.drawCenteredText(y, cd, 1); y += 10;
            } else {
                d.drawCenteredText(y, "Iniciando...", 1); y += 10;
            }
        } else if (_match.time.length() > 0) {
            d.drawCenteredText(y, _match.time.c_str(), 1); y += 10;
        }
    } else {
        String score;
        if (_match.homeScore.length() > 0 && _match.awayScore.length() > 0) {
            score = _match.homeScore + " x " + _match.awayScore;
        } else {
            score = "- x -";
        }
        d.oled().setTextSize(2);
        d.oled().setTextColor(SSD1306_WHITE);
        int16_t x1, y1; uint16_t w, h;
        d.oled().getTextBounds((String(" ") + score + String(" ")).c_str(), 0, 0, &x1, &y1, &w, &h);
        d.oled().setCursor((128 - w) / 2, y);
        d.oled().print(score.c_str());
        y += 18;

        d.oled().setTextSize(1);

        String statusLine = _match.status;
        if (_match.time.length() > 0) {
            statusLine += "  " + _match.time;
        }
        d.drawCenteredText(y, statusLine.c_str(), 1); y += 10;
    }

    if (_match.competition.length() > 0) {
        String comp = _match.competition;
        if (comp.length() > 20) comp = comp.substring(0, 19) + "~";
        d.drawCenteredText(y, comp.c_str(), 1); y += 10;
    }

    d.drawHLine(0, y, 128); y += 3;

    d.drawText(1, y, "Hold=voltar", 1);
    d.show();
}
