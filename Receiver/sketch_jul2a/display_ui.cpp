#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "config.h"
#include "display_ui.h"
#include "can_bus.h"

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

#define SCREEN_W 320
#define SCREEN_H 240

#define TOP_H    28
#define HEAD_Y   30
#define HEAD_H   22
#define ROW_Y0   57
#define ROW_H    22
#define ROWS     7
#define BOT_Y    220
#define BOT_H    20

static bool staticDrawn = false;

static uint32_t prevFrameCount = 0;
static uint32_t prevLastId[ROWS];
static uint8_t  prevLastLen[ROWS];
static uint32_t lastFpsTime = 0;
static uint32_t lastFpsFrameCount = 0;
static uint16_t rxPerSec = 0;
static uint16_t prevRxPerSec = 65535;
static uint32_t prevRxCount = 0xFFFFFFFF;
static bool prevHasFrames = false;

static void printHexByte(uint8_t value) {
  if (value < 0x10) tft.print("0");
  tft.print(value, HEX);
}

static uint16_t colorForFrame(const CanFrame& f) {
  if (!f.extended && f.id >= 0x7E8 && f.id <= 0x7EF) return UI_GREEN;
  if (f.extended) return UI_YELLOW;
  return UI_CYAN;
}

static void drawStaticScreen() {
  tft.fillScreen(UI_BG);

  // top bar
  tft.fillRect(0, 0, SCREEN_W, TOP_H, UI_HEADER);
  tft.setTextSize(2);
  tft.setTextColor(UI_TEXT);
  tft.setCursor(6, 7);
  tft.print("CAN SCANNER");

  // table header
  tft.fillRect(0, HEAD_Y, SCREEN_W, HEAD_H, UI_PANEL);
  tft.drawFastHLine(0, HEAD_Y + HEAD_H, SCREEN_W, UI_LINE);

  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED);
  tft.setCursor(8, 38);
  tft.print("ID");

  tft.setCursor(78, 38);
  tft.print("DLC");

  tft.setCursor(115, 38);
  tft.print("DATA");

  // bottom bar
  tft.fillRect(0, BOT_Y, SCREEN_W, BOT_H, UI_PANEL);
  tft.drawFastHLine(0, BOT_Y - 1, SCREEN_W, UI_LINE);

  for (uint8_t i = 0; i < ROWS; i++) {
    prevLastId[i] = 0xFFFFFFFF;
    prevLastLen[i] = 255;
  }

  prevRxCount = 0xFFFFFFFF;
  prevRxPerSec = 65535;
  prevHasFrames = !CANBus_hasFrames();

  staticDrawn = true;
}

static void updateTopNumbers() {
  uint32_t now = millis();

  if (now - lastFpsTime >= 1000) {
    uint32_t current = CANBus_getFrameCount();
    rxPerSec = current - lastFpsFrameCount;
    lastFpsFrameCount = current;
    lastFpsTime = now;
  }

  uint32_t rxCount = CANBus_getFrameCount();

  if (rxCount != prevRxCount) {
    tft.fillRect(170, 0, 80, TOP_H, UI_HEADER);
    tft.setTextSize(1);
    tft.setTextColor(UI_TEXT);
    tft.setCursor(175, 6);
    tft.print(CAN_SPEED / 1000);
    tft.print("k");

    tft.setCursor(175, 18);
    tft.print("RX:");
    tft.print(rxCount);

    prevRxCount = rxCount;
  }

  if (rxPerSec != prevRxPerSec) {
    tft.fillRect(252, 0, 68, TOP_H, UI_HEADER);
    tft.setTextSize(1);
    tft.setTextColor(UI_TEXT);
    tft.setCursor(255, 6);
    tft.print("FPS");

    tft.setCursor(255, 18);
    tft.print(rxPerSec);

    prevRxPerSec = rxPerSec;
  }
}

static bool frameDifferent(uint8_t row, const CanFrame& f) {
  if (prevLastId[row] != f.id) return true;
  if (prevLastLen[row] != f.len) return true;
  return true; // пока перерисовываем строку при каждом обновлении истории
}

static void drawEmptyRow(uint8_t row) {
  int y = ROW_Y0 + row * ROW_H;
  uint16_t bg = (row % 2 == 0) ? 0x0008 : UI_BG;
  tft.fillRect(0, y - 3, SCREEN_W, ROW_H, bg);
}

static void drawFrameRow(uint8_t row, const CanFrame& f) {
  int y = ROW_Y0 + row * ROW_H;

  drawEmptyRow(row);

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

  prevLastId[row] = f.id;
  prevLastLen[row] = f.len;
}

static void updateRows() {
  if (!CANBus_hasFrames()) {
    static bool noDataDrawn = false;

    if (!noDataDrawn) {
      tft.fillRect(0, 53, SCREEN_W, 165, UI_BG);

      tft.setTextSize(2);
      tft.setTextColor(UI_YELLOW);
      tft.setCursor(35, 105);
      tft.print("NO CAN DATA");

      tft.setTextSize(1);
      tft.setTextColor(UI_MUTED);
      tft.setCursor(45, 135);
      tft.print("Check CANH/CANL and bitrate");

      noDataDrawn = true;
    }
    return;
  }

  static bool tableWasCleared = false;

  if (!tableWasCleared) {
    tft.fillRect(0, 53, SCREEN_W, 165, UI_BG);
    tableWasCleared = true;
  }

  uint8_t count = CANBus_getHistoryCount();
  if (count > ROWS) count = ROWS;

  for (uint8_t i = 0; i < count; i++) {
    CanFrame f = CANBus_getHistoryFrame(i);
    drawFrameRow(i, f);
  }
}

static void updateBottomStatus() {
  bool hasFrames = CANBus_hasFrames();

  if (hasFrames == prevHasFrames) return;

  tft.fillRect(0, BOT_Y, SCREEN_W, BOT_H, UI_PANEL);

  tft.setTextSize(1);

  if (hasFrames) {
    tft.setTextColor(UI_GREEN);
    tft.setCursor(8, 227);
    tft.print("Status: Listening");
  } else {
    tft.setTextColor(UI_YELLOW);
    tft.setCursor(8, 227);
    tft.print("Status: Waiting CAN frames...");
  }

  tft.setTextColor(UI_MUTED);
  tft.setCursor(240, 227);
  tft.print("SPI2 TFT");

  prevHasFrames = hasFrames;
}

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
  tft.fillScreen(UI_BG);

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
  tft.fillScreen(UI_BG);

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

  staticDrawn = false;
}

void Display_drawMainScreen() {
  if (!staticDrawn) {
    drawStaticScreen();
  }

  updateTopNumbers();
  updateRows();
  updateBottomStatus();
}