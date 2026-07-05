#define HAL_CAN_MODULE_ENABLED

#include <Arduino.h>
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_can.h"

#include "config.h"
#include "can_bus.h"

static CAN_HandleTypeDef hcan;

static uint32_t frameCount = 0;

static CanFrame history[CAN_HISTORY_SIZE];
static uint8_t historyCount = 0;
static uint8_t historyHead = 0;

// ================= GPIO / CAN MSP =================

extern "C" void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle) {
  if (canHandle->Instance == CAN1) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PA12 CAN_TX
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA11 CAN_RX
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

// ================= HISTORY =================

static void pushFrame(uint32_t id, bool ext, uint8_t len, uint8_t* data) {
  CanFrame& f = history[historyHead];

  f.id = id;
  f.extended = ext;
  f.len = len;
  f.timeMs = millis();

  for (uint8_t i = 0; i < 8; i++) {
    f.data[i] = data[i];
  }

  historyHead = (historyHead + 1) % CAN_HISTORY_SIZE;

  if (historyCount < CAN_HISTORY_SIZE) {
    historyCount++;
  }

  frameCount++;
}

// ================= INIT =================

void CANBus_init() {
  hcan.Instance = CAN1;

  // APB1 обычно 36 MHz
  // 36 MHz / 4 / 18 = 500 kbit/s
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_4TQ;

  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = ENABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;

  if (HAL_CAN_Init(&hcan) != HAL_OK) {
    return;
  }

  CAN_FilterTypeDef filter = {0};

  filter.FilterBank = 0;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = 0x0000;
  filter.FilterIdLow = 0x0000;
  filter.FilterMaskIdHigh = 0x0000;
  filter.FilterMaskIdLow = 0x0000;
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) {
    return;
  }

  HAL_CAN_Start(&hcan);
}

// ================= RX =================

void CANBus_update() {
  while (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) > 0) {
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8] = {0};

    if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
      bool ext = (rxHeader.IDE == CAN_ID_EXT);
      uint32_t id = ext ? rxHeader.ExtId : rxHeader.StdId;

      pushFrame(id, ext, rxHeader.DLC, rxData);
    }
  }
}

// ================= TX =================

bool CANBus_sendStd(uint16_t id, const uint8_t* data, uint8_t len) {
  if (len > 8) len = 8;

  CAN_TxHeaderTypeDef txHeader;
  uint8_t txData[8] = {0};
  uint32_t txMailbox;

  txHeader.StdId = id;
  txHeader.ExtId = 0;
  txHeader.IDE = CAN_ID_STD;
  txHeader.RTR = CAN_RTR_DATA;
  txHeader.DLC = len;
  txHeader.TransmitGlobalTime = DISABLE;

  for (uint8_t i = 0; i < len; i++) {
    txData[i] = data[i];
  }

  return HAL_CAN_AddTxMessage(&hcan, &txHeader, txData, &txMailbox) == HAL_OK;
}

// ================= GETTERS =================

uint32_t CANBus_getFrameCount() {
  return frameCount;
}

uint8_t CANBus_getHistoryCount() {
  return historyCount;
}

bool CANBus_hasFrames() {
  return historyCount > 0;
}

CanFrame CANBus_getHistoryFrame(uint8_t index) {
  CanFrame empty = {0, 0, {0}, false, 0};

  if (index >= historyCount) {
    return empty;
  }

  int pos = (int)historyHead - 1 - index;
  if (pos < 0) pos += CAN_HISTORY_SIZE;

  return history[pos];
}