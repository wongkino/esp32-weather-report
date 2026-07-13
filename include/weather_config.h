#pragma once

#define HKO_API_BASE "https://data.weather.gov.hk/weatherAPI/opendata/weather.php"
#define HKO_LANG "tc"
#define WEATHER_REFRESH_MS 30000
#define WEATHER_HTTP_TIMEOUT_MS 10000

// 字型選擇（改 UI_FONT 這一行）
#define FONT_WQY14_CHINESE2 1
#define FONT_WQY12_CHINESE2 2
#define FONT_WQY16_CHINESE2 3
#define FONT_UNIFONT_CHINESE2 4
#define FONT_UNIFONT_CHINESE3 5
#define FONT_WQY12_GB2312B 6
#define FONT_SD_PFTC 7

// SD 卡蘋方-繁（PFTC12.vlw），見 include/font_config.h
#define UI_FONT FONT_SD_PFTC

// 分區天氣（rhrread 測站名稱須與 API 一致）
#define WEATHER_DISTRICT_LABEL "深水埗區"
#define WEATHER_TEMP_STATION "深水埗"
#define WEATHER_RAINFALL_PLACE "深水埗"
