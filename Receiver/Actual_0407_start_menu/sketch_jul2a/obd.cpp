#include "obd.h"
#include "can_bus.h"
#include "menu.h"

static ObdItem items[OBD_ITEM_COUNT] = {
  {0x04, "Engine load", "%", 0, false, 0},
  {0x05, "Coolant temp", "C", 0, false, 0},
  {0x0A, "Fuel pressure", "kPa", 0, false, 0},
  {0x0B, "MAP pressure", "kPa", 0, false, 0},
  {0x0C, "Engine RPM", "rpm", 0, false, 0},
  {0x0D, "Vehicle speed", "km/h", 0, false, 0},
  {0x0E, "Timing advance", "deg", 0, false, 0},
  {0x0F, "Intake air temp", "C", 0, false, 0},
  {0x10, "MAF air flow", "g/s", 0, false, 0},
  {0x11, "Throttle pos", "%", 0, false, 0},
  {0x1F, "Run time", "s", 0, false, 0},
  {0x21, "Distance MIL", "km", 0, false, 0},
  {0x2F, "Fuel level", "%", 0, false, 0},
  {0x31, "Dist clr DTC", "km", 0, false, 0},
  {0x33, "Baro pressure", "kPa", 0, false, 0},
  {0x42, "Control voltage", "V", 0, false, 0},
  {0x43, "Abs load", "%", 0, false, 0},
  {0x44, "Command AFR", "ratio", 0, false, 0},
  {0x45, "Rel throttle", "%", 0, false, 0},
  {0x46, "Ambient temp", "C", 0, false, 0},
  {0x47, "Abs throttle B", "%", 0, false, 0},
  {0x49, "Accel pedal D", "%", 0, false, 0},
  {0x4A, "Accel pedal E", "%", 0, false, 0},
  {0x4C, "Command throttle", "%", 0, false, 0},
  {0x51, "Fuel type", "", 0, false, 0},
  {0x52, "Ethanol fuel", "%", 0, false, 0},
  {0x5C, "Oil temp", "C", 0, false, 0},
  {0x5D, "Fuel inject", "deg", 0, false, 0},
  {0x5E, "Fuel rate", "L/h", 0, false, 0},
  {0x61, "Driver torque", "%", 0, false, 0}
};

static uint8_t requestIndex = 0;
static uint32_t lastRequest = 0;
static uint32_t lastParsedFrameCount = 0;

static void requestPID(uint8_t pid) {
  uint8_t data[8] = {0x02, 0x01, pid, 0, 0, 0, 0, 0};
  CANBus_sendStd(0x7DF, data, 8);
}

static int findPid(uint8_t pid) {
  for (uint8_t i = 0; i < OBD_ITEM_COUNT; i++) {
    if (items[i].pid == pid) return i;
  }
  return -1;
}

static float decodePID(uint8_t pid, uint8_t A, uint8_t B) {
  switch (pid) {
    case 0x04: return A * 100.0 / 255.0;
    case 0x05: return A - 40;
    case 0x0A: return A * 3;
    case 0x0B: return A;
    case 0x0C: return ((A * 256.0) + B) / 4.0;
    case 0x0D: return A;
    case 0x0E: return (A / 2.0) - 64.0;
    case 0x0F: return A - 40;
    case 0x10: return ((A * 256.0) + B) / 100.0;
    case 0x11: return A * 100.0 / 255.0;
    case 0x1F: return A * 256.0 + B;
    case 0x21: return A * 256.0 + B;
    case 0x2F: return A * 100.0 / 255.0;
    case 0x31: return A * 256.0 + B;
    case 0x33: return A;
    case 0x42: return ((A * 256.0) + B) / 1000.0;
    case 0x43: return ((A * 256.0) + B) * 100.0 / 255.0;
    case 0x44: return ((A * 256.0) + B) / 32768.0;
    case 0x45: return A * 100.0 / 255.0;
    case 0x46: return A - 40;
    case 0x47: return A * 100.0 / 255.0;
    case 0x49: return A * 100.0 / 255.0;
    case 0x4A: return A * 100.0 / 255.0;
    case 0x4C: return A * 100.0 / 255.0;
    case 0x51: return A;
    case 0x52: return A * 100.0 / 255.0;
    case 0x5C: return A - 40;
    case 0x5D: return ((A * 256.0) + B) / 128.0 - 210.0;
    case 0x5E: return ((A * 256.0) + B) * 0.05;
    case 0x61: return A - 125;
  }

  return 0;
}

void OBD_task() {
  if (Menu_getMode() != MODE_OBD2) return;

  if (millis() - lastRequest >= 220) {
    lastRequest = millis();

    requestPID(items[requestIndex].pid);

    requestIndex++;
    if (requestIndex >= OBD_ITEM_COUNT) requestIndex = 0;
  }
}

void OBD_update() {
  uint32_t fc = CANBus_getFrameCount();
  if (fc == lastParsedFrameCount) return;
  lastParsedFrameCount = fc;

  if (!CANBus_hasFrames()) return;

  CanFrame f = CANBus_getHistoryFrame(0);

  if (f.extended) return;
  if (f.id < 0x7E8 || f.id > 0x7EF) return;
  if (f.len < 4) return;

  if (f.data[1] != 0x41) return;

  uint8_t pid = f.data[2];
  uint8_t A = f.data[3];
  uint8_t B = f.data[4];

  int index = findPid(pid);
  if (index < 0) return;

  items[index].value = decodePID(pid, A, B);
  items[index].valid = true;
  items[index].lastUpdate = millis();
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