#ifndef MEDIA_BUFFER_H
#define MEDIA_BUFFER_H

#include <Arduino.h>

#define MEDIA_MAX_FILES 1
#define MEDIA_MAX_SIZE 40960

struct MediaFile {
    char name[32];
    size_t size;
    bool valid;
};

extern MediaFile mediaFile;
extern uint8_t mediaData[MEDIA_MAX_SIZE];

void mediaInit();
void mediaSet(const char* name, const uint8_t* data, size_t size);
void mediaClear();

#endif
