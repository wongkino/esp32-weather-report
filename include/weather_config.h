#pragma once

#define HKO_API_BASE "https://data.weather.gov.hk/weatherAPI/opendata/weather.php"
#define HKO_LANG "tc"
#define WEATHER_REFRESH_MS 30000
#define WEATHER_HTTP_TIMEOUT_MS 10000

// SD 卡蘋方-繁（PFTC6/10/12/18.vlw），見 include/font_config.h
// SD 掛載失敗時 main.cpp 使用 U8g2 unifont 作 fallback
