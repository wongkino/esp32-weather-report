#include "touch_cyd.h"

#include <Preferences.h>
#include <stdlib.h>

static const int TOUCH_IRQ = 36;
static const int TOUCH_MOSI = 32;
static const int TOUCH_MISO = 39;
static const int TOUCH_CLK = 25;
static const int TOUCH_CS = 33;

static const int SCREEN_W = 240;
static const int SCREEN_H = 320;

// 校準目標（略縮進，避免真的貼邊難點）
static const int16_t CAL_TX[TOUCH_CAL_POINTS] = {20, 220, 220, 20, 120};
static const int16_t CAL_TY[TOUCH_CAL_POINTS] = {20, 20, 300, 300, 160};

struct TouchCal {
  bool valid;
  int32_t a;
  int32_t b;
  int32_t c;
  int32_t d;
  int32_t e;
  int32_t f;
};

static const int32_t kTouchCalScale = 1000;

// 本機驗證通過的校準（原存於 NVS，現改為韌體預設）
static const TouchCal kDefaultCal = {true, -135, 2, 260242, 0, 179, -15422};

static TouchCal cal = kDefaultCal;
static Preferences touchPrefs;

static uint16_t calRawX[TOUCH_CAL_POINTS];
static uint16_t calRawY[TOUCH_CAL_POINTS];
static int calStepIdx = 0;
static bool calCollecting = false;

static void touchPinInit() {
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_CLK, OUTPUT);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_IRQ, INPUT);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TOUCH_CLK, LOW);
}

static void touchWrite8(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_MOSI, (data >> i) & 1);
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
}

static uint16_t touchRead12() {
  uint16_t value = 0;
  for (int i = 0; i < 12; i++) {
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    value = (value << 1) | digitalRead(TOUCH_MISO);
    digitalWrite(TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  for (int i = 0; i < 3; i++) {
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  return value;
}

static uint16_t touchSample(uint8_t command) {
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(2);
  touchWrite8(command);
  delayMicroseconds(2);
  const uint16_t value = touchRead12();
  digitalWrite(TOUCH_CS, HIGH);
  return value;
}

static int cmpU16(const void *a, const void *b) {
  return (int)(*(const uint16_t *)a) - (int)(*(const uint16_t *)b);
}

static bool calFromNvs = false;

static void loadCal() {
  // 預設使用韌體內建校準
  cal = kDefaultCal;
  calFromNvs = false;

  // 若之後有重新校準並寫入 NVS，則覆寫預設
  touchPrefs.begin("touch", true);
  const bool nvsOk = touchPrefs.getBool("ok", false);
  if (nvsOk) {
    cal.valid = true;
    cal.a = touchPrefs.getInt("a", kDefaultCal.a);
    cal.b = touchPrefs.getInt("b", kDefaultCal.b);
    cal.c = touchPrefs.getInt("c", kDefaultCal.c);
    cal.d = touchPrefs.getInt("d", kDefaultCal.d);
    cal.e = touchPrefs.getInt("e", kDefaultCal.e);
    cal.f = touchPrefs.getInt("f", kDefaultCal.f);
    calFromNvs = true;
  }
  touchPrefs.end();

  Serial.printf("[Touch] cal source=%s a=%d b=%d c=%d d=%d e=%d f=%d\n",
                calFromNvs ? "nvs" : "default", cal.a, cal.b, cal.c, cal.d, cal.e, cal.f);
}

static void saveCal() {
  touchPrefs.begin("touch", false);
  touchPrefs.putBool("ok", cal.valid);
  touchPrefs.putInt("a", cal.a);
  touchPrefs.putInt("b", cal.b);
  touchPrefs.putInt("c", cal.c);
  touchPrefs.putInt("d", cal.d);
  touchPrefs.putInt("e", cal.e);
  touchPrefs.putInt("f", cal.f);
  touchPrefs.end();
  Serial.println("[Touch] cal saved");
}

// 用四角建立仿射：sx = (a*rx+b*ry+c)/1000, sy = (d*rx+e*ry+f)/1000
static bool computeCalFromCorners() {
  // 點：0=TL 1=TR 2=BR 3=BL（中央用來驗證，不進方程）
  const int32_t rx0 = calRawX[0], ry0 = calRawY[0];
  const int32_t rx1 = calRawX[1], ry1 = calRawY[1];
  const int32_t rx3 = calRawX[3], ry3 = calRawY[3];
  const int32_t sx0 = CAL_TX[0], sy0 = CAL_TY[0];
  const int32_t sx1 = CAL_TX[1], sy1 = CAL_TY[1];
  const int32_t sx3 = CAL_TX[3], sy3 = CAL_TY[3];

  // 三點仿射（TL, TR, BL）
  // sx = a*rx + b*ry + c
  // 解 3x3（整數，結果 * scale）
  auto det3 = [](int32_t a11, int32_t a12, int32_t a13, int32_t a21, int32_t a22, int32_t a23,
                 int32_t a31, int32_t a32, int32_t a33) -> int64_t {
    return (int64_t)a11 * (a22 * a33 - a23 * a32) - (int64_t)a12 * (a21 * a33 - a23 * a31) +
           (int64_t)a13 * (a21 * a32 - a22 * a31);
  };

  const int64_t D =
      det3(rx0, ry0, 1, rx1, ry1, 1, rx3, ry3, 1);
  if (D == 0) {
    Serial.println("[Touch] cal det=0");
    return false;
  }

  const int64_t Da = det3(sx0, ry0, 1, sx1, ry1, 1, sx3, ry3, 1);
  const int64_t Db = det3(rx0, sx0, 1, rx1, sx1, 1, rx3, sx3, 1);
  const int64_t Dc = det3(rx0, ry0, sx0, rx1, ry1, sx1, rx3, ry3, sx3);
  const int64_t Dd = det3(sy0, ry0, 1, sy1, ry1, 1, sy3, ry3, 1);
  const int64_t De = det3(rx0, sy0, 1, rx1, sy1, 1, rx3, sy3, 1);
  const int64_t Df = det3(rx0, ry0, sy0, rx1, ry1, sy1, rx3, ry3, sy3);

  cal.a = (int32_t)(Da * kTouchCalScale / D);
  cal.b = (int32_t)(Db * kTouchCalScale / D);
  cal.c = (int32_t)(Dc * kTouchCalScale / D);
  cal.d = (int32_t)(Dd * kTouchCalScale / D);
  cal.e = (int32_t)(De * kTouchCalScale / D);
  cal.f = (int32_t)(Df * kTouchCalScale / D);
  cal.valid = true;

  Serial.printf("[Touch] cal a=%d b=%d c=%d d=%d e=%d f=%d\n", cal.a, cal.b, cal.c, cal.d, cal.e,
                cal.f);

  // 用中央點粗驗
  const int32_t cx =
      (cal.a * (int32_t)calRawX[4] + cal.b * (int32_t)calRawY[4] + cal.c) / kTouchCalScale;
  const int32_t cy =
      (cal.d * (int32_t)calRawX[4] + cal.e * (int32_t)calRawY[4] + cal.f) / kTouchCalScale;
  Serial.printf("[Touch] center expect %d,%d got %d,%d\n", CAL_TX[4], CAL_TY[4], (int)cx, (int)cy);

  saveCal();
  calFromNvs = true;
  return true;
}

static void mapWithCal(uint16_t avgX, uint16_t avgY, int16_t &sx, int16_t &sy) {
  if (cal.valid) {
    sx = (int16_t)((cal.a * (int32_t)avgX + cal.b * (int32_t)avgY + cal.c) / kTouchCalScale);
    sy = (int16_t)((cal.d * (int32_t)avgX + cal.e * (int32_t)avgY + cal.f) / kTouchCalScale);
  } else {
    // 後備：常見 CYD 預設
    sx = map(avgY, 200, 3800, 0, SCREEN_W - 1);
    sy = map(avgX, 200, 3800, SCREEN_H - 1, 0);
  }

  if (sx < 0) {
    sx = 0;
  }
  if (sy < 0) {
    sy = 0;
  }
  if (sx >= SCREEN_W) {
    sx = SCREEN_W - 1;
  }
  if (sy >= SCREEN_H) {
    sy = SCREEN_H - 1;
  }
}

bool touchBegin() {
  touchPinInit();
  touchSample(0xB1);
  touchSample(0xD1);
  touchSample(0x91);
  loadCal();
  return true;
}

bool touchReadRaw(uint16_t &rawX, uint16_t &rawY, uint16_t &rawZ) {
  const bool irqPressed = digitalRead(TOUCH_IRQ) == LOW;
  const int samples = 9;
  uint16_t xs[samples];
  uint16_t ys[samples];
  uint16_t zs[samples];
  int valid = 0;

  for (int i = 0; i < samples; i++) {
    const uint16_t z1 = touchSample(0xB1);
    const uint16_t rx = touchSample(0xD1);
    const uint16_t ry = touchSample(0x91);
    if (rx < 80 || ry < 80 || rx > 4000 || ry > 4000) {
      continue;
    }
    xs[valid] = rx;
    ys[valid] = ry;
    zs[valid] = z1;
    valid++;
  }
  if (valid < 4) {
    return false;
  }
  qsort(xs, valid, sizeof(uint16_t), cmpU16);
  qsort(ys, valid, sizeof(uint16_t), cmpU16);
  qsort(zs, valid, sizeof(uint16_t), cmpU16);
  rawX = xs[valid / 2];
  rawY = ys[valid / 2];
  rawZ = zs[valid / 2];
  if (!irqPressed && rawZ < 80) {
    return false;
  }
  return true;
}

bool touchReadScreen(int16_t &x, int16_t &y) {
  uint16_t rx, ry, rz;
  if (!touchReadRaw(rx, ry, rz)) {
    return false;
  }
  mapWithCal(rx, ry, x, y);
  return true;
}

void touchWaitRelease() {
  const uint32_t start = millis();
  while (millis() - start < 600) {
    delay(15);
    uint16_t rx, ry, rz;
    if (digitalRead(TOUCH_IRQ) == HIGH && !touchReadRaw(rx, ry, rz)) {
      delay(25);
      if (!touchReadRaw(rx, ry, rz)) {
        return;
      }
    }
  }
}

void touchCalClear() {
  // 清 NVS 覆寫後回到韌體預設
  touchPrefs.begin("touch", false);
  touchPrefs.clear();
  touchPrefs.end();
  cal = kDefaultCal;
  calFromNvs = false;
  calCollecting = false;
  calStepIdx = 0;
  Serial.println("[Touch] nvs cleared, using built-in default cal");
}

void touchCalCancel() {
  calCollecting = false;
  calStepIdx = 0;
  loadCal();
  Serial.println("[Touch] cal cancelled");
}

void touchCalStart() {
  calCollecting = true;
  calStepIdx = 0;
}

int touchCalStep() {
  return calCollecting ? calStepIdx : TOUCH_CAL_POINTS;
}

void touchCalTarget(int step, int16_t &tx, int16_t &ty) {
  if (step < 0 || step >= TOUCH_CAL_POINTS) {
    tx = SCREEN_W / 2;
    ty = SCREEN_H / 2;
    return;
  }
  tx = CAL_TX[step];
  ty = CAL_TY[step];
}

const char *touchCalPrompt() {
  static const char *prompts[] = {"請點左上角圓點", "請點右上角圓點", "請點右下角圓點",
                                  "請點左下角圓點", "請點中央圓點", "校準完成"};
  if (!calCollecting) {
    return prompts[5];
  }
  if (calStepIdx >= TOUCH_CAL_POINTS) {
    return prompts[5];
  }
  return prompts[calStepIdx];
}

String touchCalStatusJson() {
  // 網頁只需要狀態與提示；校準係數留在 NVS／Serial
  String json = "{";
  json += "\"calibrating\":";
  json += calCollecting ? "true" : "false";
  json += ",\"step\":";
  json += String(calCollecting ? calStepIdx : TOUCH_CAL_POINTS);
  json += ",\"points\":";
  json += String(TOUCH_CAL_POINTS);
  json += ",\"prompt\":\"";
  json += touchCalPrompt();
  json += "\",\"source\":\"";
  json += calFromNvs ? "nvs" : "default";
  json += "\",\"valid\":";
  json += cal.valid ? "true" : "false";
  json += "}";
  return json;
}

bool touchCalAddSample() {
  if (!calCollecting || calStepIdx >= TOUCH_CAL_POINTS) {
    return false;
  }
  uint16_t rx, ry, rz;
  if (!touchReadRaw(rx, ry, rz)) {
    return false;
  }
  // 多采幾次取中位
  uint16_t xs[5], ys[5];
  int n = 0;
  for (int i = 0; i < 5; i++) {
    uint16_t x2, y2, z2;
    delay(30);
    if (touchReadRaw(x2, y2, z2)) {
      xs[n] = x2;
      ys[n] = y2;
      n++;
    }
  }
  if (n < 2) {
    calRawX[calStepIdx] = rx;
    calRawY[calStepIdx] = ry;
  } else {
    qsort(xs, n, sizeof(uint16_t), cmpU16);
    qsort(ys, n, sizeof(uint16_t), cmpU16);
    calRawX[calStepIdx] = xs[n / 2];
    calRawY[calStepIdx] = ys[n / 2];
  }

  Serial.printf("[Touch] cal[%d] raw=%u,%u target=%d,%d\n", calStepIdx, calRawX[calStepIdx],
                calRawY[calStepIdx], CAL_TX[calStepIdx], CAL_TY[calStepIdx]);
  calStepIdx++;
  if (calStepIdx >= TOUCH_CAL_POINTS) {
    calCollecting = false;
    computeCalFromCorners();
  }
  return true;
}
