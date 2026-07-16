#include "districts.h"

const WeatherDistrict WEATHER_DISTRICTS[] = {
    {"深水埗區", "深水埗"},
    {"油尖旺區", "京士柏"},
    {"九龍城區", "九龍城"},
    {"黃大仙區", "黃大仙"},
    {"觀塘區", "觀塘"},
    {"中西區", "香港公園"},
    {"灣仔區", "跑馬地"},
    {"東區", "筲箕灣"},
    {"南區", "赤柱"},
    {"荃灣區", "荃灣城門谷"},
    {"屯門區", "屯門"},
    {"元朗區", "元朗公園"},
    {"北區", "打鼓嶺"},
    {"大埔區", "大埔"},
    {"沙田區", "沙田"},
    {"西貢區", "西貢"},
    {"葵青區", "青衣"},
    {"離島區", "赤鱲角"},
};

const int WEATHER_DISTRICT_COUNT =
    sizeof(WEATHER_DISTRICTS) / sizeof(WEATHER_DISTRICTS[0]);
