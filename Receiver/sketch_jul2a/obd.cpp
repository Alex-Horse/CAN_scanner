#include <Arduino.h>
#include "obd.h"
#include "can_bus.h"

// Пока заготовка под будущий OBD-II режим

void OBD_update() {
  // Здесь позже будет разбор ответов 0x7E8 / 0x7E9
}

void OBD_requestRPM() {
  uint8_t data[8] = {
    0x02, 0x01, 0x0C, 0x00,
    0x00, 0x00, 0x00, 0x00
  };

  CANBus_sendStd(0x7DF, data, 8);
}

void OBD_requestDTC() {
  uint8_t data[8] = {
    0x02, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };

  CANBus_sendStd(0x7DF, data, 8);
}