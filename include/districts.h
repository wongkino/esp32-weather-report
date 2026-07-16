#pragma once

#include <Arduino.h>

struct WeatherDistrict {
  const char *label;
  const char *tempStation;
};

// 顯示名／溫度測站（須與 HKO rhrread 一致）；定義見 src/districts.cpp
extern const WeatherDistrict WEATHER_DISTRICTS[];
extern const int WEATHER_DISTRICT_COUNT;

// 將 index 限制在 [0, WEATHER_DISTRICT_COUNT)
inline int clampDistrictIndex(int index) {
  if (index < 0 || index >= WEATHER_DISTRICT_COUNT) {
    return 0;
  }
  return index;
}
