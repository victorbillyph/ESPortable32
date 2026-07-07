#include "MediaBuffer.h"

MediaFile mediaFile;
uint8_t mediaData[MEDIA_MAX_SIZE];

void mediaInit() {
    mediaFile.valid = false;
    mediaFile.size = 0;
    mediaFile.name[0] = '\0';
}

void mediaSet(const char* name, const uint8_t* data, size_t size) {
    if (size > MEDIA_MAX_SIZE) size = MEDIA_MAX_SIZE;
    mediaFile.size = size;
    mediaFile.valid = true;
    memcpy(mediaData, data, size);
    strncpy(mediaFile.name, name, sizeof(mediaFile.name) - 1);
    mediaFile.name[sizeof(mediaFile.name) - 1] = '\0';
}

void mediaClear() {
    mediaFile.valid = false;
    mediaFile.size = 0;
    mediaFile.name[0] = '\0';
}
