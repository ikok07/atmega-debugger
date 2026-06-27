#include "nvic_defs.h"
#include "stm32f401xc.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_spi.h"

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    // GPIO configuration is done in spi.c because of frequent changes
  }
}