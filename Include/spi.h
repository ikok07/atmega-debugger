#ifndef INCLUDE_SPI
#define INCLUDE_SPI

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include <stddef.h>
#include <stdint.h>

#define SPI1_DEFAULT_TARGET_FREQUENCY_HZ                        125000               // Might not be exactly this frequency 
                                                                                // due to prescaler mechanism

HAL_StatusTypeDef SPI_Init();
HAL_StatusTypeDef SPI_SetFrequency(uint32_t FrequencyHz);

void SPI_EnableIO();
void SPI_DisableIO();

#endif /* INCLUDE_SPI */
