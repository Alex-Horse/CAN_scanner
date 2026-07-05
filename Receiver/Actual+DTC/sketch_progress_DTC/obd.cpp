#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "obd.h"
#include "can_bus.h"
#include "menu.h"

static ObdScreen currentScreen = OBD_SCREEN_DASHBOARD;
static uint8_t obdScroll = 0;

static uint8_t requestIndex = 0;
static uint32_t lastRequestMs = 0;
static uint32_t lastParsedFrameCount = 0;

static char vin[18] = "";
static char calId[24] = "";
static char cvn[16] = "";

static ObdDtc dtcList[OBD_DTC_COUNT];
static ObdRawLine rawLines[OBD_RAW_COUNT];

static ObdItem items[OBD_ITEM_COUNT] = {
  {0x04, "Engine load",      "%",    0, false, 0},
  {0x05, "Coolant temp",     "C",    0, false, 0},
  {0x0A, "Fuel pressure",    "kPa",  0, false, 0},
  {0x0B, "MAP pressure",     "kPa",  0, false, 0},
  {0x0C, "Engine RPM",       "rpm",  0, false, 0},
  {0x0D, "Vehicle speed",    "km/h", 0, false, 0},
  {0x0E, "Timing advance",   "deg",  0, false, 0},
  {0x0F, "Intake air temp",  "C",    0, false, 0},
  {0x10, "MAF air flow",     "g/s",  0, false, 0},
  {0x11, "Throttle pos",     "%",    0, false, 0},
  {0x1F, "Run time",         "s",    0, false, 0},
  {0x21, "Distance MIL",     "km",   0, false, 0},
  {0x2F, "Fuel level",       "%",    0, false, 0},
  {0x31, "Dist clr DTC",     "km",   0, false, 0},
  {0x33, "Baro pressure",    "kPa",  0, false, 0},
  {0x42, "Control voltage",  "V",    0, false, 0},
  {0x43, "Abs load",         "%",    0, false, 0},
  {0x44, "Command AFR",      "ratio",0, false, 0},
  {0x45, "Rel throttle",     "%",    0, false, 0},
  {0x46, "Ambient temp",     "C",    0, false, 0},
  {0x47, "Abs throttle B",   "%",    0, false, 0},
  {0x49, "Accel pedal D",    "%",    0, false, 0},
  {0x4A, "Accel pedal E",    "%",    0, false, 0},
  {0x4C, "Command throttle", "%",    0, false, 0},
  {0x51, "Fuel type",        "",     0, false, 0},
  {0x52, "Ethanol fuel",     "%",    0, false, 0},
  {0x5C, "Oil temp",         "C",    0, false, 0},
  {0x5D, "Fuel inject",      "deg",  0, false, 0},
  {0x5E, "Fuel rate",        "L/h",  0, false, 0},
  {0x61, "Driver torque",    "%",    0, false, 0}
};

static void addRawLine(const char* text) {
  for (int i = OBD_RAW_COUNT - 1; i > 0; i--) {
    strncpy(rawLines[i].text, rawLines[i - 1].text, sizeof(rawLines[i].text));
    rawLines[i].text[sizeof(rawLines[i].text) - 1] = '\0';
  }

  strncpy(rawLines[0].text, text, sizeof(rawLines[0].text));
  rawLines[0].text[sizeof(rawLines[0].text) - 1] = '\0';
}

static void addRawTx(uint16_t id, const uint8_t* data, uint8_t len) {
  char line[48];
  char buf[4];

  snprintf(line, sizeof(line), "TX %03X ", id);

  for (uint8_t i = 0; i < len && i < 8; i++) {
    snprintf(buf, sizeof(buf), "%02X ", data[i]);
    strncat(line, buf, sizeof(line) - strlen(line) - 1);
  }

  addRawLine(line);
}

static void addRawRx(const CanFrame& f) {
  char line[48];
  char buf[4];

  snprintf(line, sizeof(line), "RX %03lX ", (unsigned long)f.id);

  for (uint8_t i = 0; i < f.len && i < 8; i++) {
    snprintf(buf, sizeof(buf), "%02X ", f.data[i]);
    strncat(line, buf, sizeof(line) - strlen(line) - 1);
  }

  addRawLine(line);
}

static void requestModePid(uint8_t mode, uint8_t pid) {
  uint8_t data[8] = {0x02, mode, pid, 0, 0, 0, 0, 0};
  CANBus_sendStd(0x7DF, data, 8);
  addRawTx(0x7DF, data, 8);
}

static int findPid(uint8_t pid) {
  for (uint8_t i = 0; i < OBD_ITEM_COUNT; i++) {
    if (items[i].pid == pid) return i;
  }
  return -1;
}

static float decodePID(uint8_t pid, uint8_t A, uint8_t B) {
  switch (pid) {
    case 0x04: return A * 100.0f / 255.0f;
    case 0x05: return A - 40;
    case 0x0A: return A * 3;
    case 0x0B: return A;
    case 0x0C: return ((A * 256.0f) + B) / 4.0f;
    case 0x0D: return A;
    case 0x0E: return (A / 2.0f) - 64.0f;
    case 0x0F: return A - 40;
    case 0x10: return ((A * 256.0f) + B) / 100.0f;
    case 0x11: return A * 100.0f / 255.0f;
    case 0x1F: return A * 256.0f + B;
    case 0x21: return A * 256.0f + B;
    case 0x2F: return A * 100.0f / 255.0f;
    case 0x31: return A * 256.0f + B;
    case 0x33: return A;
    case 0x42: return ((A * 256.0f) + B) / 1000.0f;
    case 0x43: return ((A * 256.0f) + B) * 100.0f / 255.0f;
    case 0x44: return ((A * 256.0f) + B) / 32768.0f;
    case 0x45: return A * 100.0f / 255.0f;
    case 0x46: return A - 40;
    case 0x47: return A * 100.0f / 255.0f;
    case 0x49: return A * 100.0f / 255.0f;
    case 0x4A: return A * 100.0f / 255.0f;
    case 0x4C: return A * 100.0f / 255.0f;
    case 0x51: return A;
    case 0x52: return A * 100.0f / 255.0f;
    case 0x5C: return A - 40;
    case 0x5D: return ((A * 256.0f) + B) / 128.0f - 210.0f;
    case 0x5E: return ((A * 256.0f) + B) * 0.05f;
    case 0x61: return A - 125;
  }

  return 0;
}

static void decodeDtcBytes(uint8_t a, uint8_t b, char* out) {
  const char typeChars[] = {'P', 'C', 'B', 'U'};

  uint8_t type = (a & 0xC0) >> 6;
  uint8_t digit1 = (a & 0x30) >> 4;
  uint8_t digit2 = (a & 0x0F);
  uint8_t digit3 = (b & 0xF0) >> 4;
  uint8_t digit4 = (b & 0x0F);

  snprintf(out, 6, "%c%u%X%X%X",
           typeChars[type],
           digit1,
           digit2,
           digit3,
           digit4);
}

static void parseMode01(const CanFrame& f) {
  if (f.len < 4) return;
  if (f.data[1] != 0x41) return;

  uint8_t pid = f.data[2];
  uint8_t A = f.data[3];
  uint8_t B = (f.len > 4) ? f.data[4] : 0;

  int index = findPid(pid);
  if (index < 0) return;

  items[index].value = decodePID(pid, A, B);
  items[index].valid = true;
  items[index].lastUpdate = millis();
}

static void parseDtcResponse(const CanFrame& f) {
  if (f.len < 4) return;
  if (f.data[1] != 0x43) return;

  uint8_t dtcIndex = 0;

  for (uint8_t i = 2; i + 1 < f.len && dtcIndex < OBD_DTC_COUNT; i += 2) {
    uint8_t a = f.data[i];
    uint8_t b = f.data[i + 1];

    if (a == 0x00 && b == 0x00) continue;

    decodeDtcBytes(a, b, dtcList[dtcIndex].code);
    dtcList[dtcIndex].valid = true;
    dtcIndex++;
  }
}

static void parseVinSingleFrame(const CanFrame& f) {
  // упрощенный вариант: часть авто отвечает VIN многофреймово, это добавим позже
  if (f.len < 5) return;
  if (f.data[1] != 0x49) return;
  if (f.data[2] != 0x02) return;

  uint8_t pos = 0;

  for (uint8_t i = 3; i < f.len && pos < 17; i++) {
    if (f.data[i] >= 32 && f.data[i] <= 126) {
      vin[pos++] = (char)f.data[i];
    }
  }

  vin[pos] = '\0';
}

void OBD_init() {
  currentScreen = OBD_SCREEN_DASHBOARD;
  obdScroll = 0;
  requestIndex = 0;
  lastRequestMs = 0;
  lastParsedFrameCount = 0;

  vin[0] = '\0';
  calId[0] = '\0';
  cvn[0] = '\0';

  for (uint8_t i = 0; i < OBD_DTC_COUNT; i++) {
    dtcList[i].valid = false;
    strcpy(dtcList[i].code, "");
  }

  for (uint8_t i = 0; i < OBD_RAW_COUNT; i++) {
    rawLines[i].text[0] = '\0';
  }
}

void OBD_task() {
  if (Menu_getMode() != MODE_OBD2) return;

  if (millis() - lastRequestMs < 220) return;
  lastRequestMs = millis();

  if (currentScreen == OBD_SCREEN_DTC) {
    OBD_requestDTC();
    return;
  }

  if (currentScreen == OBD_SCREEN_VEHICLE_INFO) {
    OBD_requestVIN();
    return;
  }

  requestModePid(0x01, items[requestIndex].pid);

  requestIndex++;
  if (requestIndex >= OBD_ITEM_COUNT) requestIndex = 0;
}

void OBD_update() {
  uint32_t fc = CANBus_getFrameCount();
  if (fc == lastParsedFrameCount) return;
  lastParsedFrameCount = fc;

  if (!CANBus_hasFrames()) return;

  CanFrame f = CANBus_getHistoryFrame(0);

  if (f.extended) return;
  if (f.id < 0x7E8 || f.id > 0x7EF) return;

  addRawRx(f);

  if (f.len < 3) return;

  if (f.data[1] == 0x41) {
    parseMode01(f);
  } else if (f.data[1] == 0x43) {
    parseDtcResponse(f);
  } else if (f.data[1] == 0x49) {
    parseVinSingleFrame(f);
  }
}

void OBD_nextScreen() {
  currentScreen = (ObdScreen)((uint8_t)currentScreen + 1);
  if (currentScreen > OBD_SCREEN_RAW) {
    currentScreen = OBD_SCREEN_DASHBOARD;
  }
}

void OBD_prevScreen() {
  if (currentScreen == OBD_SCREEN_DASHBOARD) {
    currentScreen = OBD_SCREEN_RAW;
  } else {
    currentScreen = (ObdScreen)((uint8_t)currentScreen - 1);
  }
}

void OBD_scrollUp() {
  if (obdScroll > 0) obdScroll--;
}

void OBD_scrollDown() {
  if (obdScroll + 10 < OBD_ITEM_COUNT) obdScroll++;
}

ObdScreen OBD_getScreen() {
  return currentScreen;
}

uint8_t OBD_getScroll() {
  return obdScroll;
}

ObdItem OBD_getItem(uint8_t index) {
  if (index >= OBD_ITEM_COUNT) {
    ObdItem empty = {0, "", "", 0, false, 0};
    return empty;
  }

  return items[index];
}

uint8_t OBD_getItemCount() {
  return OBD_ITEM_COUNT;
}

ObdDtc OBD_getDtc(uint8_t index) {
  if (index >= OBD_DTC_COUNT) {
    ObdDtc empty;
    empty.valid = false;
    strcpy(empty.code, "");
    return empty;
  }

  return dtcList[index];
}

uint8_t OBD_getDtcCount() {
  return OBD_DTC_COUNT;
}

ObdRawLine OBD_getRawLine(uint8_t index) {
  if (index >= OBD_RAW_COUNT) {
    ObdRawLine empty;
    empty.text[0] = '\0';
    return empty;
  }

  return rawLines[index];
}

uint8_t OBD_getRawCount() {
  return OBD_RAW_COUNT;
}

const char* OBD_getVin() {
  return vin[0] ? vin : "---";
}

const char* OBD_getCalId() {
  return calId[0] ? calId : "---";
}

const char* OBD_getCvn() {
  return cvn[0] ? cvn : "---";
}

void OBD_requestDTC() {
  uint8_t data[8] = {0x02, 0x03, 0, 0, 0, 0, 0, 0};
  CANBus_sendStd(0x7DF, data, 8);
  addRawTx(0x7DF, data, 8);
}

void OBD_requestVIN() {
  uint8_t data[8] = {0x02, 0x09, 0x02, 0, 0, 0, 0, 0};
  CANBus_sendStd(0x7DF, data, 8);
  addRawTx(0x7DF, data, 8);
}