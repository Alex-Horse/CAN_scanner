#pragma once

#include <Arduino.h>

#define CAN_HISTORY_SIZE 8

struct CanFrame {
  uint32_t id;
  uint8_t len;
  uint8_t data[8];
  bool extended;
  uint32_t timeMs;
};

void CANBus_init();
void CANBus_update();

uint32_t CANBus_getFrameCount();
uint8_t CANBus_getHistoryCount();
CanFrame CANBus_getHistoryFrame(uint8_t index);
bool CANBus_hasFrames();

bool CANBus_sendStd(uint16_t id, const uint8_t* data, uint8_t len);

