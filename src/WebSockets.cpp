#include "WebSockets.h"

WebSockets::WebSockets() : _ws("/ws"), _dataCallback(nullptr) {}

void WebSockets::begin(AsyncWebServer& server) {
    _ws.onEvent(onEvent);
    server.addHandler(&_ws);
    Serial.println("[WS] WebSocket handler registered at /ws");
}

void WebSockets::broadcast(const String& msg) {
    _ws.textAll(msg);
}

void WebSockets::handleMessage(AsyncWebSocketClient* client, const String& msg) {
    if (_dataCallback) {
        _dataCallback(client, msg);
    }
}

void WebSockets::setDataCallback(void (*cb)(AsyncWebSocketClient*, const String&)) {
    _dataCallback = cb;
}

AsyncWebSocket* WebSockets::getSocket() { return &_ws; }

void WebSockets::onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client connected: %u\n", client->id());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client disconnected: %u\n", client->id());
            break;
        case WS_EVT_DATA: {
            String msg = String((char*)data, len);
            // Find the WebSockets instance to forward the message
            // This is a static method - we handle it differently
            break;
        }
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}
