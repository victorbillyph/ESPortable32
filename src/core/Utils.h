#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

class Utils {
public:
    static String formatBytes(size_t bytes);
    static String formatUptime(unsigned long ms);
    static String formatRSSI(int rssi);
    static String secToString(int sec);
};

#endif
