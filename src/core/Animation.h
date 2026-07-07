#ifndef ANIMATION_H
#define ANIMATION_H

#include <Arduino.h>

class Animation {
public:
    static int easeInOut(int t, int total, int maxVal);
    static int slide(int t, int total, int maxVal);
};

#endif
