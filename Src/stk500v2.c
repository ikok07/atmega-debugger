#include "stk500v2.h"
#include "usb.h"
#include <stdint.h>
#include <string.h>

static USB_MessageStatusTypeDef parse_cmd(uint8_t *Buf, uint32_t *Len);

static STK500V2_MessageTypeDef gStk500Message = {0};

USB_MessageParserTypeDef STK500_Parser = {.ParseCmd = parse_cmd};
USB_MessageStatusTypeDef parse_cmd(uint8_t *Buf, uint32_t *Len) {
  if (*Len > sizeof(STK500V2_MessageTypeDef)) {
    return USB_MESSAGE_PARSE_ERR;
  }

  uint16_t correct_checksum = 0;
  // Last byte is the checksum, so exclude it
  for (uint16_t i = 0; i < *Len - 1; i++) {
    correct_checksum ^= Buf[i];
  }

  if (correct_checksum != Buf[*Len - 1])
    return USB_MESSAGE_PARSE_ERR;

  // TODO: Should account for body array size
  memcpy(&gStk500Message, Buf, *Len);

  return USB_MESSAGE_OK;
}