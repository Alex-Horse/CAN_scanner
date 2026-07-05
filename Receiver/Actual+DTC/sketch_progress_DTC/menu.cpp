#include "menu.h"
#include "buttons.h"

static bool menuOpen = false;
static uint8_t selected = 0;
static AppMode currentMode = MODE_CAN_BUS;

void Menu_init() {
  Buttons_init();
}

void Menu_update() {
  ButtonEvent ev = Buttons_update();

  if (ev == BTN_NONE) return;

  if (ev == BTN_MENU_EVENT) {
    menuOpen = !menuOpen;
    return;
  }

  if (!menuOpen) return;

  if (ev == BTN_UP_EVENT) {
    if (selected == 0) selected = 2;
    else selected--;
  }

  if (ev == BTN_DOWN_EVENT) {
    selected++;
    if (selected > 2) selected = 0;
  }

  if (ev == BTN_OK_EVENT) {
  if (selected == 0) {
    currentMode = MODE_OBD2;
    menuOpen = false;
  }

  if (selected == 1) {
    currentMode = MODE_CAN_BUS;
    menuOpen = false;
  }

  if (selected == 2) {
    currentMode = MODE_DTC;
    menuOpen = false;
  }
}
}

bool Menu_isOpen() {
  return menuOpen;
}

uint8_t Menu_getSelected() {
  return selected;
}

AppMode Menu_getMode() {
  return currentMode;
}