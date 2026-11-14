#pragma once
#include "IPAddress.h"
constexpr int WL_CONNECTED = 3;
struct WiFiStub {
    int status() const { return WL_CONNECTED; }
    IPAddress localIP() const { return {}; }
};
inline WiFiStub WiFi;
