#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

struct WarningLabel {
  const char *code;
  const char *label;
};

// HKO warnsum code → 畫面簡稱
static const WarningLabel WARNING_LABELS[] = {
    {"WRAINA", "黃雨"},
    {"WRAINR", "紅雨"},
    {"WRAINB", "黑雨"},
    {"WFIREY", "黃火"},
    {"WFIRER", "紅火"},
    {"TC1", "一號風球"},
    {"TC3", "三號風球"},
    {"TC9", "九號風球"},
    {"TC10", "十號風球"},
    {"WTS", "雷暴"},
    {"WHOT", "酷熱"},
    {"WCOLD", "寒冷"},
    {"WMSGNL", "強烈季候風"},
    {"WL", "山泥"},
    {"WFNTSA", "北區水浸"},
    {"WFROST", "霜凍"},
    {"WTMW", "海嘯"},
    {"WTCPRE8", "預警八號"},
};

inline String warningShortLabel(const char *code, const char *name) {
  if (code == nullptr) {
    code = "";
  }
  // TC8NE / TC8SE / TC8SW / TC8NW
  if (strncmp(code, "TC8", 3) == 0) {
    return String("八號風球");
  }
  for (const WarningLabel &entry : WARNING_LABELS) {
    if (strcmp(code, entry.code) == 0) {
      return String(entry.label);
    }
  }
  if (name != nullptr && name[0] != '\0') {
    return String(name);
  }
  return String(code);
}

inline String joinActiveWarningNames(JsonObject warnsumRoot) {
  String result;
  for (JsonPair kv : warnsumRoot) {
    JsonObject item = kv.value().as<JsonObject>();
    if (item.isNull()) {
      continue;
    }
    const char *action = item["actionCode"] | "";
    if (strcmp(action, "CANCEL") == 0 || strcmp(action, "CANCEL_ALL") == 0 ||
        strcmp(action, "END") == 0) {
      continue;
    }
    // warnsum 的 key 即為 code；item["code"] 可能缺
    const char *code = item["code"] | kv.key().c_str();
    const char *name = item["name"] | "";
    const String label = warningShortLabel(code, name);
    if (label.isEmpty()) {
      continue;
    }
    if (result.length() > 0) {
      result += " ";
    }
    result += label;
  }
  return result;
}
