#include <Arduino.h>
#include <U8g2lib.h>

// ================= OLED SSD1306 =================
// SDA -> PA0
// SCL -> PA1

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,
  PA1,
  PA0,
  U8X8_PIN_NONE
);

// ================= CAN PINS =================
// CAN_RX -> PA11
// CAN_TX -> PA12
// SN65HVD230:
// TXD/CTX <- PA12
// RXD/CRX -> PA11
// CANH -> CANH
// CANL -> CANL
// GND  -> GND

uint32_t sendCount = 0;
uint32_t failCount = 0;

uint32_t lastId = 0;
uint8_t lastLen = 0;
uint8_t lastData[8] = {0};

// ================= DISPLAY =================

void drawHexByte(uint8_t value) {
  if (value < 0x10) u8g2.print("0");
  u8g2.print(value, HEX);
}

void drawStartup() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "STM32 CAN Generator");
  u8g2.drawStr(0, 30, "OLED: PA0/PA1");
  u8g2.drawStr(0, 44, "CAN: PA11/PA12");
  u8g2.drawStr(0, 58, "Direct bxCAN init");
  u8g2.sendBuffer();
}

void drawCanOk() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 14, "CAN init OK");
  u8g2.drawStr(0, 30, "Speed: 500 kbps");
  u8g2.drawStr(0, 46, "No STM32_CAN lib");
  u8g2.sendBuffer();
}

void drawCanFail() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 14, "CAN init FAILED");
  u8g2.drawStr(0, 32, "Check board/core");
  u8g2.drawStr(0, 50, "F103C8Tx required");
  u8g2.sendBuffer();
}

void drawScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(0, 9, "STM32 CAN Generator");
  u8g2.drawHLine(0, 12, 128);

  u8g2.setCursor(0, 24);
  u8g2.print("500kbps Sent:");
  u8g2.print(sendCount);

  u8g2.setCursor(0, 34);
  u8g2.print("Fail:");
  u8g2.print(failCount);

  u8g2.setCursor(0, 44);
  u8g2.print("ID: STD 0x");
  u8g2.print(lastId, HEX);

  u8g2.setCursor(0, 54);
  u8g2.print("DLC:");
  u8g2.print(lastLen);
  u8g2.print(" ");

  for (uint8_t i = 0; i < lastLen && i < 4; i++) {
    drawHexByte(lastData[i]);
    u8g2.print(" ");
  }

  u8g2.setCursor(0, 64);
  for (uint8_t i = 4; i < lastLen && i < 8; i++) {
    drawHexByte(lastData[i]);
    u8g2.print(" ");
  }

  u8g2.sendBuffer();
}

// ================= DIRECT bxCAN DRIVER =================

bool waitBitSet(volatile uint32_t &reg, uint32_t mask, uint32_t timeoutMs) {
  uint32_t t = millis();
  while ((reg & mask) == 0) {
    if (millis() - t > timeoutMs) return false;
  }
  return true;
}

bool waitBitClear(volatile uint32_t &reg, uint32_t mask, uint32_t timeoutMs) {
  uint32_t t = millis();
  while (reg & mask) {
    if (millis() - t > timeoutMs) return false;
  }
  return true;
}

bool canInit500k() {
  // GPIOA + AFIO clock
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;

  // CAN1 clock
  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

  // PA11 = CAN_RX input pull-up
  GPIOA->CRH &= ~(0xF << 12);
  GPIOA->CRH |=  (0x8 << 12);
  GPIOA->ODR |=  (1 << 11);

  // PA12 = CAN_TX alternate function push-pull 50 MHz
  GPIOA->CRH &= ~(0xF << 16);
  GPIOA->CRH |=  (0xB << 16);

  // No CAN remap: PA11 / PA12
#ifdef AFIO_MAPR_CAN_REMAP
  AFIO->MAPR &= ~AFIO_MAPR_CAN_REMAP;
#endif

  // Enter initialization mode
  CAN1->MCR |= CAN_MCR_INRQ;
  if (!waitBitSet(CAN1->MSR, CAN_MSR_INAK, 100)) return false;

  // Normal mode, auto retransmission enabled
  CAN1->MCR = CAN_MCR_INRQ | CAN_MCR_ABOM;

  // 500 kbps при APB1 = 36 MHz:
  // 36 MHz / prescaler 4 / 18 tq = 500 kbit/s
  // SJW = 1 tq, BS1 = 13 tq, BS2 = 4 tq
  CAN1->BTR =
      (0 << 24) |   // SJW  = 1 tq
      (3 << 20) |   // BS2  = 4 tq
      (12 << 16) |  // BS1  = 13 tq
      (3 << 0);     // Prescaler = 4

  // Leave initialization mode
  CAN1->MCR &= ~CAN_MCR_INRQ;
  if (!waitBitClear(CAN1->MSR, CAN_MSR_INAK, 100)) return false;

  return true;
}

bool canSendStd(uint16_t id, uint8_t *data, uint8_t len) {
  if (len > 8) len = 8;

  uint8_t mb;

  if (CAN1->TSR & CAN_TSR_TME0) {
    mb = 0;
  } else if (CAN1->TSR & CAN_TSR_TME1) {
    mb = 1;
  } else if (CAN1->TSR & CAN_TSR_TME2) {
    mb = 2;
  } else {
    return false;
  }

  CAN_TxMailBox_TypeDef *mailbox = &CAN1->sTxMailBox[mb];

  mailbox->TIR = 0;
  mailbox->TDTR = len & 0x0F;

  mailbox->TDLR =
    ((uint32_t)data[0] << 0)  |
    ((uint32_t)data[1] << 8)  |
    ((uint32_t)data[2] << 16) |
    ((uint32_t)data[3] << 24);

  mailbox->TDHR =
    ((uint32_t)data[4] << 0)  |
    ((uint32_t)data[5] << 8)  |
    ((uint32_t)data[6] << 16) |
    ((uint32_t)data[7] << 24);

  // Standard 11-bit ID
  mailbox->TIR = ((uint32_t)(id & 0x7FF) << 21);

  // Request transmission
  mailbox->TIR |= CAN_TI0R_TXRQ;

  return true;
}

// ================= TEST FRAMES =================

void sendTestFrame() {
  static uint8_t frameIndex = 0;
  static uint8_t counter = 0;

  uint16_t id;
  uint8_t data[8];

  if (frameIndex == 0) {
    id = 0x100;
    data[0] = counter;
    data[1] = 0x11;
    data[2] = 0x22;
    data[3] = 0x33;
    data[4] = 0x44;
    data[5] = 0x55;
    data[6] = 0x66;
    data[7] = 0x77;
  } else if (frameIndex == 1) {
    id = 0x200;
    data[0] = 0xAA;
    data[1] = 0xBB;
    data[2] = counter;
    data[3] = counter + 1;
    data[4] = counter + 2;
    data[5] = counter + 3;
    data[6] = 0xCC;
    data[7] = 0xDD;
  } else {
    id = 0x321;
    data[0] = 0xDE;
    data[1] = 0xAD;
    data[2] = 0xBE;
    data[3] = 0xEF;
    data[4] = counter;
    data[5] = 0x01;
    data[6] = 0x02;
    data[7] = 0x03;
  }

  if (canSendStd(id, data, 8)) {
    sendCount++;
    lastId = id;
    lastLen = 8;

    for (uint8_t i = 0; i < 8; i++) {
      lastData[i] = data[i];
    }
  } else {
    failCount++;
  }

  counter++;

  frameIndex++;
  if (frameIndex > 2) frameIndex = 0;
}

// ================= SETUP / LOOP =================

void setup() {
  delay(300);

  u8g2.begin();
  drawStartup();
  delay(800);

  bool ok = canInit500k();

  if (ok) {
    drawCanOk();
  } else {
    drawCanFail();
    while (1) {
      delay(1000);
    }
  }

  delay(800);
}

void loop() {
  static uint32_t lastSend = 0;
  static uint32_t lastDraw = 0;

  // CAN frame every 100 ms
  if (millis() - lastSend >= 100) {
    lastSend = millis();
    sendTestFrame();
  }

  // OLED refresh 10 Hz
  if (millis() - lastDraw >= 100) {
    lastDraw = millis();
    drawScreen();
  }
}