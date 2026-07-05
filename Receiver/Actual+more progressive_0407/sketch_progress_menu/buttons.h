#pragma once
#include <Arduino.h>

enum ButtonEvent {
  BTN_NONE,
  BTN_MENU_EVENT,
  BTN_UP_EVENT,
  BTN_DOWN_EVENT,
  BTN_OK_EVENT
};

void Buttons_init();
ButtonEvent Buttons_update();