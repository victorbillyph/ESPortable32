#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

class WebSockets {
public:
    WebSockets();
    void begin(AsyncWebServer& server);
    void broadcast(const String& msg);
    void handleMessage(AsyncWebSocketClient* client, const String& msg);
    void setDataCallback(void (*cb)(AsyncWebSocketClient*, const String&));
    AsyncWebSocket* getSocket();

private:
    AsyncWebSocket _ws;
    void (*_dataCallback)(AsyncWebSocketClient*, const String&);

    static void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len);
};
