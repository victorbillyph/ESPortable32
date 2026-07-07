#include "Animation.h"

int Animation::easeInOut(int t, int total, int maxVal) {
    if (t <= 0) return 0;
    if (t >= total) return maxVal;
    float p = (float)t / total;
    if (p < 0.5f) return (int)(maxVal * 2.0f * p * p);
    return (int)(maxVal * (1.0f - (-2.0f * p + 2.0f) * (-2.0f * p + 2.0f) / 2.0f));
}

int Animation::slide(int t, int total, int maxVal) {
    if (t <= 0) return 0;
    if (t >= total) return maxVal;
    return (int)((float)maxVal * t / total);
}
