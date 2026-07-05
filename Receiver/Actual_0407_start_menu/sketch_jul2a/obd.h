#pragma once
#include <Arduino.h>

#define OBD_ITEM_COUNT 30

struct ObdItem {
  uint8_t pid;
  const char* name;
  const char* unit;
  float value;
  bool valid;
  uint32_t lastUpdate;
};

void OBD_update();
void OBD_task();
ObdItem OBD_getItem(uint8_t index);
uint8_t OBD_getItemCount();