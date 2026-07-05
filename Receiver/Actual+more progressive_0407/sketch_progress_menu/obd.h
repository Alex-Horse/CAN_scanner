#pragma once
#include <Arduino.h>

#define OBD_ITEM_COUNT 30
#define OBD_DTC_COUNT 8
#define OBD_RAW_COUNT 6

enum ObdScreen {
  OBD_SCREEN_DASHBOARD,
  OBD_SCREEN_PID_LIST,
  OBD_SCREEN_DTC,
  OBD_SCREEN_FREEZE,
  OBD_SCREEN_READINESS,
  OBD_SCREEN_VEHICLE_INFO,
  OBD_SCREEN_RAW
};

struct ObdItem {
  uint8_t pid;
  const char* name;
  const char* unit;
  float value;
  bool valid;
  uint32_t lastUpdate;
};

struct ObdDtc {
  char code[6];
  bool valid;
};

struct ObdRawLine {
  char text[48];
};

void OBD_init();
void OBD_update();
void OBD_task();

void OBD_nextScreen();
void OBD_prevScreen();
void OBD_scrollUp();
void OBD_scrollDown();

ObdScreen OBD_getScreen();
uint8_t OBD_getScroll();

ObdItem OBD_getItem(uint8_t index);
uint8_t OBD_getItemCount();

ObdDtc OBD_getDtc(uint8_t index);
uint8_t OBD_getDtcCount();

ObdRawLine OBD_getRawLine(uint8_t index);
uint8_t OBD_getRawCount();

const char* OBD_getVin();
const char* OBD_getCalId();
const char* OBD_getCvn();

void OBD_requestDTC();
void OBD_requestVIN();