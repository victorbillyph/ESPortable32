#include "Utils.h"

String Utils::formatBytes(size_t bytes) {
    if (bytes < 1024) return String(bytes) + "B";
    if (bytes < 1024 * 1024) return String(bytes / 1024) + "KB";
    return String(bytes / (1024 * 1024)) + "MB";
}

String Utils::formatUptime(unsigned long ms) {
    unsigned long s = ms / 1000;
    int h = s / 3600;
    int m = (s % 3600) / 60;
    int sec = s % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, sec);
    return String(buf);
}

String Utils::formatRSSI(int rssi) {
    if (rssi >= -50) return "Excellent";
    if (rssi >= -60) return "Good";
    if (rssi >= -70) return "Fair";
    return "Weak";
}

String Utils::secToString(int sec) {
    if (sec < 60) return String(sec) + "s";
    if (sec < 3600) return String(sec / 60) + "m " + String(sec % 60) + "s";
    return String(sec / 3600) + "h " + String((sec % 3600) / 60) + "m";
}
