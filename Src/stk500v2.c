#include "stk500v2.h"
#include "app_state.h"
#include "gpio_defs.h"
#include "ringbuf.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_spi.h"
#include "usb.h"
#include "usbd_def.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ------ Command handlers ------ */
static uint8_t enter_prog_mode(STK500V2_CommandTypeDef *Stk500Command);
static void leave_prog_mode(STK500V2_CommandTypeDef *Stk500Command);

/* ------ Utilities ------ */
static USB_CommandStatusTypeDef
send_response(STK500V2_CommandTypeDef *Stk500Command, uint8_t Status,
              uint8_t *Data, size_t Len);

static uint8_t generate_checksum(uint8_t *Buffer, uint16_t Len);
static uint8_t validate_checksum(uint8_t *Buffer, uint16_t Len);

static uint8_t get_parameter_value(uint8_t ParamID);
static uint8_t set_parameter_value(uint8_t ParamID, uint8_t Value);

static uint8_t get_sck_period_value_from_freq(uint32_t Frequency);
static uint32_t get_freq_from_sck_period_value(uint8_t Value);

static void avr_enable_reset();
static void avr_disable_reset();

static uint8_t gCommandBuffer[MIN_COMMAND_SIZE];
static uint8_t gCurrentSckDuration = 0xFF;

STK500V2_ParamPairTypeDef Stk500V2_StaticParams[PARAMS_COUNT] = {
    {.ParamID = PARAM_HW_VER, .Value = HW_VERSION},
    {.ParamID = PARAM_SW_MAJOR, .Value = SW_MAJOR_VERSION},
    {.ParamID = PARAM_SW_MINOR, .Value = SW_MINOR_VERSION},
    {.ParamID = PARAM_VTARGET, .Value = VOLTAGE_TARGET},
};

USB_CommandStatusTypeDef
STK500V2_HandleCmd(STK500V2_CommandTypeDef *Stk500Command) {
  USB_CommandStatusTypeDef status = USB_COMMAND_OK;

  uint8_t cmd_id = Stk500Command->MessageBody[0];
  if (cmd_id == CMD_SIGN_ON) {

    // Make sure MCU is not in programming mode
    avr_disable_reset();

    uint8_t data[9];
    data[0] = strlen(DEBUGGER_SIGNATURE);
    memcpy(data + 1, DEBUGGER_SIGNATURE, data[0]);
    status = send_response(Stk500Command, STATUS_CMD_OK, data, sizeof(data));

  } else if (cmd_id == CMD_GET_PARAMETER) {

    uint8_t param_id = Stk500Command->MessageBody[1];
    uint8_t data = get_parameter_value(param_id);
    status = send_response(Stk500Command, STATUS_CMD_OK, &data, 1);

  } else if (cmd_id == CMD_SET_PARAMETER) {

    uint8_t param_id = Stk500Command->MessageBody[1];
    uint8_t param_value = Stk500Command->MessageBody[2];
    uint8_t rc = set_parameter_value(param_id, param_value);
    status = send_response(Stk500Command,
                           rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK, NULL, 0);
  } else if (cmd_id == CMD_ENTER_PROGMODE_ISP) {
    uint8_t rc = enter_prog_mode(Stk500Command);
    uint8_t resp_status = rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK;
    if (rc == 2)
      resp_status = STATUS_CMD_TOUT;
    status = send_response(Stk500Command, resp_status, NULL, 0);
  } else if (cmd_id == CMD_LEAVE_PROGMODE_ISP) {
    leave_prog_mode(Stk500Command);
    status = send_response(Stk500Command, STATUS_CMD_OK, NULL, 0);
  }

  return status;
}

USB_CommandStatusTypeDef
STK500V2_ParseCmd(ringbuf_t RingBuffer,
                  STK500V2_CommandTypeDef *Stk500Command) {
  USB_CommandStatusTypeDef status = USB_COMMAND_OK;

  STK500V2_CommandTypeDef cmd;
  uint16_t len = ringbuf_bytes_used(RingBuffer);

  // Check the minimum required bytes for an STK500 command
  if (len < MIN_COMMAND_SIZE) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  }

  // Extract only the header bytes for now
  uint8_t header[MIN_COMMAND_SIZE];
  ringbuf_peek(RingBuffer, header, MIN_COMMAND_SIZE);

  // Check wheter this is STK500 command
  if (header[0] != STK500V2_START_BYTE) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  }

  cmd.SequenceNumber = header[1];
  cmd.MessageSize = (header[2] << 8) | header[3];
  cmd.Token = header[4];

  // After we know the whole command size we can validate the whole command
  uint16_t cmd_total_size =
      cmd.MessageSize + MIN_COMMAND_SIZE + 1; // Account for the checksum byte
  if (len < cmd_total_size) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  }

  // If command is valid, copy it from the ringbuffer
  ringbuf_memcpy_from(gCommandBuffer, RingBuffer, cmd_total_size);

  // Validate the checksum
  if (!validate_checksum(gCommandBuffer, cmd_total_size)) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  };

  // Extract the command payload
  cmd.MessageBody = gCommandBuffer + 5;
  cmd.Checksum = gCommandBuffer[cmd.MessageSize + MIN_COMMAND_SIZE];

  // Assign new command to passed pointer
  memcpy(Stk500Command, &cmd, sizeof(cmd));

  return USB_COMMAND_OK;
}

uint8_t enter_prog_mode(STK500V2_CommandTypeDef *Stk500Command) {
  STK500V2_EnterProgModeBodyTypeDef body;
  memcpy(&body, Stk500Command->MessageBody,
         sizeof(STK500V2_EnterProgModeBodyTypeDef));

  // Enable SPI GPIOs
  SPI_EnableIO();

  // Enter programming mode by pulling RESET to low
  avr_enable_reset();

  // Stabilization delay
  HAL_Delay(body.StabDelay);

  // Pull down RESET to activate programming mode
  HAL_GPIO_WritePin(GPIO_PORT_AVR_RESET, GPIO_PIN_AVR_RESET, GPIO_PIN_RESET);
  HAL_Delay(body.CmdExecutionDelay);

  // Send commands
  uint8_t rx_buf[4];
  HAL_StatusTypeDef hal_err;
  uint8_t success = 0;
  for (uint8_t i = 0; i < body.SyncLoops; i++) {
    if ((hal_err =
             HAL_SPI_TransmitReceive(&gAppState.hspi1, body.Commands, rx_buf,
                                     sizeof(body.Commands), 100)) != HAL_OK) {
      SPI_DisableIO();
      return hal_err == HAL_TIMEOUT ? 2 : 1;
    };
    if (rx_buf[body.PollIndex] == body.PollValue) {
      success = 1;
      break;
    }
  }

  if (!success)
    return 1;

  return 0;
}

void leave_prog_mode(STK500V2_CommandTypeDef *Stk500Command) {
  uint8_t pre_delay = Stk500Command->MessageBody[1];
  uint8_t post_delay = Stk500Command->MessageBody[2];

  HAL_Delay(pre_delay);

  avr_disable_reset();
  SPI_DisableIO();

  HAL_Delay(post_delay);
}

USB_CommandStatusTypeDef send_response(STK500V2_CommandTypeDef *Stk500Command,
                                       uint8_t Status, uint8_t *Data,
                                       size_t Len) {
  size_t body_len = Len + 2;
  uint8_t body[body_len];
  body[0] = Stk500Command->MessageBody[0]; // Answer ID = Command ID
  body[1] = Status;
  memcpy(body + 2, Data, Len);

  // Header + Body + Checksum
  uint16_t response_len = MIN_COMMAND_SIZE + body_len + 1;
  uint8_t response[response_len];
  response[0] = STK500V2_START_BYTE;
  response[1] = Stk500Command->SequenceNumber; // Sequence number
  response[2] = (body_len >> 8) & 0xFF;
  response[3] = body_len & 0xFF;
  response[4] = Stk500Command->Token;
  memcpy(response + 5, body, body_len);
  response[MIN_COMMAND_SIZE + body_len] =
      generate_checksum(response, response_len);

  USBD_StatusTypeDef usbd_err;
  if ((usbd_err = USB_SendData(response, response_len)) != USBD_OK) {
    return USB_COMMAND_RESPONSE_ERR;
  }

  return USB_COMMAND_OK;
}

uint8_t generate_checksum(uint8_t *Buffer, uint16_t Len) {
  uint16_t correct_checksum = 0;
  // Last byte is the checksum, so exclude it
  for (uint16_t i = 0; i < Len - 1; i++) {
    correct_checksum ^= Buffer[i];
  }

  return correct_checksum;
}

uint8_t validate_checksum(uint8_t *Buffer, uint16_t Len) {
  uint16_t correct_checksum = generate_checksum(Buffer, Len);
  return correct_checksum == Buffer[Len - 1];
}

/**
  @brief Get parameter value by parameter id
  @returns Parameter value. (if parameter is unsupported, a zero is returned)
*/
uint8_t get_parameter_value(uint8_t ParamID) {
  if (ParamID <= PARAM_VTARGET) {
    for (uint8_t i = 0;
         i < sizeof(Stk500V2_StaticParams) / sizeof(Stk500V2_StaticParams[0]);
         i++) {
      STK500V2_ParamPairTypeDef pair = Stk500V2_StaticParams[i];
      if (pair.ParamID == ParamID) {
        return pair.Value;
      }
    }
  } else if (ParamID == PARAM_SCK_DURATION) {
    if (gCurrentSckDuration == 0xFF) {
      gCurrentSckDuration =
          get_sck_period_value_from_freq(SPI1_DEFAULT_TARGET_FREQUENCY_HZ);
    }
    return gCurrentSckDuration;
  }

  return 0x00;
}

uint8_t set_parameter_value(uint8_t ParamID, uint8_t Value) {
  HAL_StatusTypeDef hal_err;
  if (ParamID == PARAM_SCK_DURATION) {
    gCurrentSckDuration = Value;

    if ((hal_err = SPI_SetFrequency(get_freq_from_sck_period_value(Value))) !=
        HAL_OK) {
      return 1;
    };
    return 0;
  }

  return 0; // Ok if parameter is not supported
}

uint8_t get_sck_period_value_from_freq(uint32_t Frequency) {
  // Algorithm directly inferred from avrdude's source code (stk500v2.c,
  // stk500v2_set_sck_period)

  uint32_t value;

  if (Frequency >= STK500V2_XTAL / 4.0f) {
    value = 0;
  } else if (Frequency >= STK500V2_XTAL / 16.0f) {
    value = 1;
  } else if (Frequency >= STK500V2_XTAL / 64.0f) {
    value = 2;
  } else if (Frequency >= STK500V2_XTAL / 128.0f) {
    value = 3;
  } else {
    // Rearange of this equation (
    // (unsigned int) ceil(1/(24*f/(double) my.xtal) - 10.0/12.0)
    // ) to make it integer only:
    value = (1843200 + Frequency - 1) / (6 * Frequency);
  }

  if (value >= 255)
    return 254;

  return value;
}
uint32_t get_freq_from_sck_period_value(uint8_t Value) {
  if (Value == 0) {
    return STK500V2_XTAL / 4.0f;
  } else if (Value == 1) {
    return STK500V2_XTAL / 16.0f;
  } else if (Value == 2) {
    return STK500V2_XTAL / 64.0f;
  } else if (Value == 3) {
    return STK500V2_XTAL / 128.0f;
  } else {
    return (1843200 + (3 * Value) + 2) / ((6 * Value) + 5);
  }
}

void avr_enable_reset() {
  GPIO_InitTypeDef gpio_conf = {
      .Mode = GPIO_MODE_OUTPUT_PP,
      .Pin = GPIO_PIN_AVR_RESET,
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_LOW,
  };
  HAL_GPIO_Init(GPIO_PORT_AVR_RESET, &gpio_conf);
  HAL_GPIO_WritePin(GPIO_PORT_AVR_RESET, GPIO_PIN_AVR_RESET, GPIO_PIN_SET);
}

void avr_disable_reset() {
  GPIO_InitTypeDef gpio_conf = {
      .Mode = GPIO_MODE_INPUT,
      .Pin = GPIO_PIN_AVR_RESET,
      .Pull = GPIO_PULLUP,
      .Speed = GPIO_SPEED_FREQ_LOW,
  };
  // Reset into input mode pull-up
  HAL_GPIO_Init(GPIO_PORT_AVR_RESET, &gpio_conf);
}