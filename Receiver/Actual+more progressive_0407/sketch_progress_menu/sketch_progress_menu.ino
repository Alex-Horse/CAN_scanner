#include <Arduino.h>
#include "config.h"
#include "can_bus.h"
#include "display_ui.h"
#include "obd.h"
#include "menu.h"
#include "settings.h"

void setup() {
  delay(300);

  Settings_init();
  Menu_init();

OBD_init();

  Display_init();
  Display_showStartup();

  delay(800);

  CANBus_init();

  Display_showCanOk();

  delay(800);
}


void loop() {
  CANBus_update();
  Menu_update();

  OBD_task();
  OBD_update();

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw >= DISPLAY_REFRESH_MS) {
    lastDraw = millis();
    Display_drawMainScreen();
  }
}