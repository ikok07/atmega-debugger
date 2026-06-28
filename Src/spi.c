#include "spi.h"
#include "app_state.h"
#include "gpio_defs.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_spi.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  uint32_t HALConstant;
  uint16_t DivisionFactor;
} SPI_PrescalerMapping;

HAL_StatusTypeDef SPI_Init() {
  HAL_StatusTypeDef hal_err;
  gAppState.hspi1 = (SPI_HandleTypeDef){.Instance = SPI1,
                                        .Init = {
                                            .Mode = SPI_MODE_MASTER,
                                            .CLKPhase = SPI_PHASE_1EDGE,
                                            .CLKPolarity = SPI_POLARITY_LOW,
                                            .DataSize = SPI_DATASIZE_8BIT,
                                            .Direction = SPI_DIRECTION_2LINES,
                                            .FirstBit = SPI_FIRSTBIT_MSB,
                                            .NSS = SPI_NSS_SOFT,
                                            .TIMode = SPI_TIMODE_DISABLE,
                                        }};

  // Peripheral is initialized inside the method
  if ((hal_err = SPI_SetFrequency(SPI1_DEFAULT_TARGET_FREQUENCY_HZ)) !=
      HAL_OK) {
    return hal_err;
  }

  return hal_err;
}

/**
    @note This will no set the SPI frequency exactly as provided, instead it
   will try to reach the closest possible frequency
*/
HAL_StatusTypeDef SPI_SetFrequency(uint32_t FrequencyHz) {
  HAL_StatusTypeDef hal_err;
  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

  SPI_PrescalerMapping presc_mappings[] = {
      {SPI_BAUDRATEPRESCALER_2, 2},     {SPI_BAUDRATEPRESCALER_4, 4},
      {SPI_BAUDRATEPRESCALER_8, 8},     {SPI_BAUDRATEPRESCALER_16, 16},
      {SPI_BAUDRATEPRESCALER_32, 32},   {SPI_BAUDRATEPRESCALER_64, 64},
      {SPI_BAUDRATEPRESCALER_128, 128}, {SPI_BAUDRATEPRESCALER_256, 256},
  };

  uint32_t closest_freq_diff = INT_MAX;
  uint8_t closest_prescaler = presc_mappings[0].HALConstant;

  for (uint8_t i = 0; i < sizeof(presc_mappings) / sizeof(presc_mappings[0]);
       i++) {
    SPI_PrescalerMapping presc = presc_mappings[i];
    uint32_t freq = pclk2 / (presc.DivisionFactor);
    uint32_t diff;

    if (FrequencyHz >= freq) {
      diff = FrequencyHz - freq;
    } else {
      diff = freq - FrequencyHz;
    }

    if (diff < closest_freq_diff) {
      closest_freq_diff = diff;
      closest_prescaler = presc.HALConstant;
    }
  }

  gAppState.hspi1.Init.BaudRatePrescaler = closest_prescaler;

  __HAL_SPI_DISABLE(&gAppState.hspi1);

  if ((hal_err = HAL_SPI_Init(&gAppState.hspi1)) != HAL_OK) {
    return hal_err;
  }

  return hal_err;
}

void SPI_EnableIO() {
  GPIO_InitTypeDef gpio_conf = {.Mode = GPIO_MODE_AF_PP,
                                .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
                                .Pull = GPIO_NOPULL,
                                .Alternate = GPIO_AF5_SPI1};

  gpio_conf.Pin = GPIO_PIN_SPI_SCK;
  HAL_GPIO_Init(GPIO_PORT_SPI_SCK, &gpio_conf);

  gpio_conf.Pin = GPIO_PIN_SPI_MOSI;
  HAL_GPIO_Init(GPIO_PORT_SPI_MOSI, &gpio_conf);

  gpio_conf.Pull = GPIO_PULLUP;
  gpio_conf.Pin = GPIO_PIN_SPI_MISO;
  HAL_GPIO_Init(GPIO_PORT_SPI_MISO, &gpio_conf);
}

void SPI_DisableIO() {
  GPIO_InitTypeDef gpio_conf = {.Mode = GPIO_MODE_INPUT,
                                .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
                                .Pull = GPIO_PULLUP,
                                .Pin = GPIO_PIN_AVR_RESET};

  // MISO into input mode pull-up
  gpio_conf.Pin = GPIO_PIN_SPI_MISO;
  HAL_GPIO_Init(GPIO_PORT_SPI_MISO, &gpio_conf);

  // MOSI into input mode no pull
  gpio_conf.Pull = GPIO_NOPULL;
  gpio_conf.Pin = GPIO_PIN_SPI_MOSI;
  HAL_GPIO_Init(GPIO_PORT_SPI_MOSI, &gpio_conf);

  // SCK into input mode pull-down
  gpio_conf.Pull = GPIO_PULLDOWN;
  gpio_conf.Pin = GPIO_PIN_SPI_SCK;
  HAL_GPIO_Init(GPIO_PORT_SPI_SCK, &gpio_conf);
}
