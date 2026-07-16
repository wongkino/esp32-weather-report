#pragma once

#include <Arduino.h>

bool wifiTryConnect();
bool wifiStartConfigPortal(void (*onStatus)(const char *title, const char *detail));
bool wifiIsConnected();

// 地區（Preferences namespace weather / idx）
int wifiLoadDistrictIndex();
void wifiSaveDistrictIndex(int index);

// 已連線後在裝置 IP:80 提供設定頁（改地區）
typedef void (*WifiDistrictChangedFn)(int index);
typedef void (*WifiTouchCalFn)();
void wifiSetDistrictChangedCallback(WifiDistrictChangedFn cb);
void wifiSetTouchCalStartCallback(WifiTouchCalFn cb);
void wifiSetTouchCalResetCallback(WifiTouchCalFn cb);
void wifiSetTouchCalDoneCallback(WifiTouchCalFn cb);
void wifiStartSettingsServer();
void wifiHandleClient();
