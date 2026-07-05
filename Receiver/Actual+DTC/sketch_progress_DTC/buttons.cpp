#include "buttons.h"
#include "config.h"

#define DEBOUNCE_MS 40

struct Btn {
  uint8_t pin;
  bool stableState;
  bool lastRead;
  uint32_t lastChange;
};

static Btn buttons[] = {
  {BTN_MENU, HIGH, HIGH, 0},
  {BTN_UP,   HIGH, HIGH, 0},
  {BTN_DOWN, HIGH, HIGH, 0},
  {BTN_OK,   HIGH, HIGH, 0}
};

void Buttons_init() {
  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK,   INPUT_PULLUP);
}

ButtonEvent Buttons_update() {
  for (uint8_t i = 0; i < 4; i++) {
    bool r = digitalRead(buttons[i].pin);

    if (r != buttons[i].lastRead) {
      buttons[i].lastRead = r;
      buttons[i].lastChange = millis();
    }

    if ((millis() - buttons[i].lastChange) > DEBOUNCE_MS) {
      if (r != buttons[i].stableState) {
        buttons[i].stableState = r;

        if (r == LOW) {
          if (i == 0) return BTN_MENU_EVENT;
          if (i == 1) return BTN_UP_EVENT;
          if (i == 2) return BTN_DOWN_EVENT;
          if (i == 3) return BTN_OK_EVENT;
        }
      }
    }
  }

  return BTN_NONE;
}