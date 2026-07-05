#pragma once

#include <Arduino.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   PB12
#define TFT_DC   PB10
#define TFT_RST  PB11

#define CAN_SPEED 500000
#define DISPLAY_REFRESH_MS 100

#define UI_BG       ILI9341_BLACK
#define UI_HEADER   0x0015
#define UI_PANEL    0x0841
#define UI_TEXT     ILI9341_WHITE
#define UI_MUTED    0x8410
#define UI_CYAN     ILI9341_CYAN
#define UI_GREEN    ILI9341_GREEN
#define UI_YELLOW   ILI9341_YELLOW
#define UI_RED      ILI9341_RED
#define UI_LINE     0x3186
