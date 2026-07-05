#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "config.h"
#include "display_ui.h"
#include "can_bus.h"
#include "menu.h"
#include "obd.h"

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// ================= SCREEN =================

#define SCREEN_W 320
#define SCREEN_H 240

#define TOP_H    28
#define BOT_Y    220
#define BOT_H    20

#define CAN_ROW_Y0 57
#define CAN_ROW_H  22
#define CAN_ROWS   7

#define OBD_ROW_Y0 38
#define OBD_ROW_H  18
#define OBD_ROWS   10

#define OBD_DATA_ROWS 7
#define RAW_Y0       168
#define RAW_ROW_H    10
#define RAW_ROWS     4

enum ScreenId {
  SCREEN_NONE,
  SCREEN_CAN,
  SCREEN_MENU,
  SCREEN_OBD,
  SCREEN_DTC
};

static ScreenId currentScreen = SCREEN_NONE;

// ================= CAN CACHE =================

static uint32_t prevCanRx = 0xFFFFFFFF;
static uint16_t prevCanFps = 65535;
static uint32_t fpsLastTime = 0;
static uint32_t fpsLastCount = 0;
static uint16_t canFps = 0;

static char prevRawText[RAW_ROWS][48];

// ================= MENU CACHE =================

static int8_t prevMenuSelected = -1;

// ================= OBD CACHE =================

static bool prevObdValid[OBD_ITEM_COUNT];
static float prevObdValue[OBD_ITEM_COUNT];

// ================= HELPERS =================

static void printHexByte(uint8_t v) {
  if (v < 0x10) tft.print("0");
  tft.print(v, HEX);
}

static uint16_t rowBg(uint8_t row) {
  return (row % 2 == 0) ? 0x0008 : UI_BG;
}

static uint16_t colorForFrame(const CanFrame& f) {
  if (!f.extended && f.id >= 0x7E8 && f.id <= 0x7EF) return UI_GREEN;
  if (f.extended) return UI_YELLOW;
  return UI_CYAN;
}

static void clearScreen() {
  tft.fillScreen(UI_BG);
}

// ================= COMMON =================

static void drawTopTitle(const char* title) {
  tft.fillRect(0, 0, SCREEN_W, TOP_H, UI_HEADER);
  tft.setTextColor(UI_TEXT);
  tft.setTextSize(2);
  tft.setCursor(6, 7);
  tft.print(title);
}

static void drawBottomText(const char* left, const char* right, uint16_t colorLeft) {
  tft.fillRect(0, BOT_Y, SCREEN_W, BOT_H, UI_PANEL);
  tft.drawFastHLine(0, BOT_Y - 1, SCREEN_W, UI_LINE);

  tft.setTextSize(1);
  tft.setTextColor(colorLeft);
  tft.setCursor(8, 227);
  tft.print(left);

  tft.setTextColor(UI_MUTED);
  tft.setCursor(240, 227);
  tft.print(right);
}

// ================= CAN SCREEN =================

static void drawDtcScreen() {
  if (currentScreen != SCREEN_DTC) {
    currentScreen = SCREEN_DTC;

    clearScreen();
    drawTopTitle("DTC ERRORS");

    tft.setTextSize(1);
    tft.setTextColor(UI_MUTED);
    tft.setCursor(210, 8);
    tft.print("Mode 03");

    tft.fillRect(0, 30, 320, 22, UI_PANEL);
    tft.setTextColor(UI_MUTED);
    tft.setCursor(8, 38);
    tft.print("Stored trouble codes");

    drawBottomText("OK: request DTC", "OBD2", UI_MUTED);
  }

  bool hasAny = false;

  for (uint8_t i = 0; i < OBD_DTC_COUNT; i++) {
    ObdDtc d = OBD_getDtc(i);

    int y = 65 + i * 18;
    tft.fillRect(0, y - 2, 320, 16, rowBg(i));

    tft.setTextSize(1);

    if (d.valid) {
      hasAny = true;
      tft.setTextColor(UI_RED);
      tft.setCursor(20, y);
      tft.print(d.code);
    } else {
      tft.setTextColor(UI_MUTED);
      tft.setCursor(20, y);
      tft.print("---");
    }
  }

  if (!hasAny) {
    tft.setTextSize(2);
    tft.setTextColor(UI_GREEN);
    tft.setCursor(70, 115);
    tft.print("NO DTC DATA");
  }
}

static void drawCanStatic() {
  clearScreen();

  drawTopTitle("CAN SCANNER");

  tft.fillRect(0, 30, SCREEN_W, 22, UI_PANEL);
  tft.drawFastHLine(0, 52, SCREEN_W, UI_LINE);

  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED);

  tft.setCursor(8, 38);
  tft.print("ID");

  tft.setCursor(78, 38);
  tft.print("DLC");

  tft.setCursor(115, 38);
  tft.print("DATA");

  drawBottomText("Status: Waiting CAN frames...", "SPI2 TFT", UI_YELLOW);

  prevCanRx = 0xFFFFFFFF;
  prevCanFps = 65535;
}

static void updateCanTop() {
  uint32_t now = millis();

  if (now - fpsLastTime >= 1000) {
    uint32_t count = CANBus_getFrameCount();
    canFps = count - fpsLastCount;
    fpsLastCount = count;
    fpsLastTime = now;
  }

  uint32_t rx = CANBus_getFrameCount();

  if (rx != prevCanRx) {
    tft.fillRect(170, 0, 80, TOP_H, UI_HEADER);
    tft.setTextSize(1);
    tft.setTextColor(UI_TEXT);

    tft.setCursor(175, 6);
    tft.print(CAN_SPEED / 1000);
    tft.print("k");

    tft.setCursor(175, 18);
    tft.print("RX:");
    tft.print(rx);

    prevCanRx = rx;
  }

  if (canFps != prevCanFps) {
    tft.fillRect(252, 0, 68, TOP_H, UI_HEADER);
    tft.setTextSize(1);
    tft.setTextColor(UI_TEXT);

    tft.setCursor(255, 6);
    tft.print("FPS");

    tft.setCursor(255, 18);
    tft.print(canFps);

    prevCanFps = canFps;
  }
}

static void drawCanRow(uint8_t row, const CanFrame& f) {
  int y = CAN_ROW_Y0 + row * CAN_ROW_H;

  tft.fillRect(0, y - 3, SCREEN_W, CAN_ROW_H, rowBg(row));

  tft.setTextSize(1);

  tft.setTextColor(colorForFrame(f));
  tft.setCursor(8, y);
  tft.print(f.extended ? "X " : "S ");
  tft.print("0x");
  tft.print(f.id, HEX);

  tft.setTextColor(UI_TEXT);
  tft.setCursor(82, y);
  tft.print(f.len);

  tft.setCursor(115, y);
  for (uint8_t i = 0; i < f.len && i < 8; i++) {
    printHexByte(f.data[i]);
    tft.print(" ");
  }
}

static void updateCanRows() {
  static uint32_t lastDrawnCount = 0;

  uint32_t countNow = CANBus_getFrameCount();
  if (countNow == lastDrawnCount) return;

  lastDrawnCount = countNow;

  if (!CANBus_hasFrames()) return;

  uint8_t count = CANBus_getHistoryCount();
  if (count > CAN_ROWS) count = CAN_ROWS;

  for (uint8_t i = 0; i < count; i++) {
    drawCanRow(i, CANBus_getHistoryFrame(i));
  }

  drawBottomText("Status: Listening", "SPI2 TFT", UI_GREEN);
}

static void drawCanScreen() {
  if (currentScreen != SCREEN_CAN) {
    currentScreen = SCREEN_CAN;
    drawCanStatic();
  }

  updateCanTop();
  updateCanRows();
}

// ================= MENU SCREEN =================

static void drawMenuItem(uint8_t i, bool selected) {
  const char* items[] = {
    "OBD2 scanner",
    "CAN bus mode",
    "DTC errors"
  };

  int y = 60 + i * 40;

  if (selected) {
    tft.fillRect(20, y - 8, 280, 30, UI_CYAN);
    tft.setTextColor(ILI9341_BLACK);
  } else {
    tft.fillRect(20, y - 8, 280, 30, UI_BG);
    tft.setTextColor(UI_TEXT);
  }

  tft.setTextSize(2);
  tft.setCursor(35, y);
  tft.print(items[i]);
}

static void drawMenuStatic() {
  clearScreen();

  drawTopTitle("MAIN MENU");

  for (uint8_t i = 0; i < 3; i++) {
    drawMenuItem(i, i == Menu_getSelected());
  }

  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED);
  tft.setCursor(20, 220);
  tft.print("UP/DOWN select  OK confirm");

  prevMenuSelected = Menu_getSelected();
}

static void drawMenuScreen() {
  if (currentScreen != SCREEN_MENU) {
    currentScreen = SCREEN_MENU;
    prevMenuSelected = -1;
    drawMenuStatic();
    return;
  }

  uint8_t selected = Menu_getSelected();

  if (selected != prevMenuSelected) {
    if (prevMenuSelected >= 0) {
      drawMenuItem(prevMenuSelected, false);
    }

    drawMenuItem(selected, true);
    prevMenuSelected = selected;
  }
}

// ================= OBD SCREEN =================

static void drawObdStatic() {
  clearScreen();

  drawTopTitle("OBD2 SCANNER");

  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED);
  tft.setCursor(230, 8);
  tft.print("500k");

  // Основные OBD данные
  for (uint8_t i = 0; i < OBD_DATA_ROWS; i++) {
    int y = OBD_ROW_Y0 + i * OBD_ROW_H;

    tft.fillRect(0, y - 2, SCREEN_W, 16, rowBg(i));

    ObdItem item = OBD_getItem(i);

    tft.setTextSize(1);
    tft.setTextColor(UI_CYAN);
    tft.setCursor(6, y);
    tft.print(item.name);

    prevObdValid[i] = !item.valid;
    prevObdValue[i] = -999999.0;
  }

  // RAW CAN блок
  tft.drawFastHLine(0, 154, SCREEN_W, UI_LINE);

  tft.setTextSize(1);
  tft.setTextColor(UI_YELLOW);
  tft.setCursor(6, 158);
  tft.print("RAW CAN TX/RX:");

  tft.fillRect(0, RAW_Y0 - 2, SCREEN_W, 45, UI_BG);

  drawBottomText("Data kept after disconnect", "OBD2", UI_MUTED);
}

static void drawRawCanBlock() {
  for (uint8_t i = 0; i < RAW_ROWS; i++) {
    ObdRawLine line = OBD_getRawLine(i);

    if (strcmp(prevRawText[i], line.text) == 0) {
      continue;
    }

    int y = RAW_Y0 + i * RAW_ROW_H;

    tft.fillRect(0, y - 1, SCREEN_W, RAW_ROW_H, UI_BG);

    tft.setTextSize(1);

    if (strncmp(line.text, "TX", 2) == 0) {
      tft.setTextColor(UI_YELLOW);
    } else if (strncmp(line.text, "RX", 2) == 0) {
      tft.setTextColor(UI_GREEN);
    } else {
      tft.setTextColor(UI_MUTED);
    }

    tft.setCursor(6, y);
    tft.print(line.text);

    strncpy(prevRawText[i], line.text, sizeof(prevRawText[i]));
    prevRawText[i][sizeof(prevRawText[i]) - 1] = '\0';
  }
}

static void drawObdValue(uint8_t i) {
  ObdItem item = OBD_getItem(i);
  int y = OBD_ROW_Y0 + i * OBD_ROW_H;

  tft.fillRect(175, y - 2, 140, 16, rowBg(i));

  tft.setTextSize(1);
  tft.setTextColor(item.valid ? UI_GREEN : UI_MUTED);
  tft.setCursor(175, y);

  if (item.valid) {
    tft.print(item.value, 1);
    tft.print(" ");
    tft.print(item.unit);
  } else {
    tft.print("---");
  }

  prevObdValid[i] = item.valid;
  prevObdValue[i] = item.value;
}

static void drawObdScreen() {
  if (currentScreen != SCREEN_OBD) {
    currentScreen = SCREEN_OBD;

    for (uint8_t i = 0; i < RAW_ROWS; i++) {
      prevRawText[i][0] = '\0';
    }

    drawObdStatic();
  }

  for (uint8_t i = 0; i < OBD_DATA_ROWS; i++) {
    ObdItem item = OBD_getItem(i);

    bool changed =
      item.valid != prevObdValid[i] ||
      fabs(item.value - prevObdValue[i]) > 0.05;

    if (changed) {
      drawObdValue(i);
    }
  }

  drawRawCanBlock();
}

// ================= PUBLIC =================

void Display_init() {
  SPI.setMOSI(PB15);
  SPI.setMISO(PB14);
  SPI.setSCLK(PB13);
  SPI.begin();

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(UI_BG);
}

void Display_showStartup() {
  currentScreen = SCREEN_NONE;

  clearScreen();

  tft.fillRect(0, 0, 320, 40, UI_HEADER);

  tft.setTextColor(UI_TEXT);
  tft.setTextSize(2);
  tft.setCursor(12, 12);
  tft.print("STM32 CAN Scanner");

  tft.setTextSize(1);
  tft.setCursor(20, 65);
  tft.print("Display: ILI9341 240x320");

  tft.setCursor(20, 85);
  tft.print("SPI: Hardware SPI2");

  tft.setCursor(20, 105);
  tft.print("SCK PB13  MOSI PB15");

  tft.setCursor(20, 125);
  tft.print("CAN: PA11 / PA12");

  tft.setTextColor(UI_YELLOW);
  tft.setCursor(20, 160);
  tft.print("Initializing...");
}



void Display_showCanOk() {
  currentScreen = SCREEN_NONE;

  clearScreen();

  tft.fillRect(0, 0, 320, 40, UI_HEADER);

  tft.setTextColor(UI_GREEN);
  tft.setTextSize(3);
  tft.setCursor(20, 65);
  tft.print("CAN OK");

  tft.setTextColor(UI_TEXT);
  tft.setTextSize(2);
  tft.setCursor(20, 110);
  tft.print("500 kbps");

  tft.setTextSize(1);
  tft.setCursor(20, 150);
  tft.print("Waiting CAN frames...");
}

void Display_drawMainScreen() {
  if (Menu_isOpen()) {
    drawMenuScreen();
    return;
  }

  if (Menu_getMode() == MODE_OBD2) {
    drawObdScreen();
    return;
  }

  if (Menu_getMode() == MODE_DTC) {
  drawDtcScreen();
  return;
  }

  drawCanScreen();
}



