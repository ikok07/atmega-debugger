#include "usb.h"
#include "app_state.h"
#include "nvic_defs.h"
#include "stk500v2.h"
#include "usbd_cdc.h"
#include "usbd_cdc_cb.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include <stdint.h>

static USB_MessageStatusTypeDef parse_cmd(uint8_t *Buf, uint32_t *Len);

USBD_StatusTypeDef USB_Init() {
  USBD_StatusTypeDef usb_err;
  if ((usb_err = USBD_Init(&gAppState.husb.husbd, &FS_Desc, 0)) != USBD_OK) {
    return usb_err;
  }

  // Enable interrupts only after husbd is initialized to make sure pData is not
  // NULL
  HAL_NVIC_SetPriority(OTG_FS_IRQn, NVIC_PRIORITY_OTG_FS, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

  if ((usb_err = USBD_RegisterClass(&gAppState.husb.husbd, &USBD_CDC)) !=
      USBD_OK) {
    return usb_err;
  }

  if ((usb_err = USBD_CDC_RegisterInterface(
           &gAppState.husb.husbd, &USBD_Interface_fops_FS)) != USBD_OK) {
    return usb_err;
  }

  if ((usb_err = USBD_Start(&gAppState.husb.husbd)) != USBD_OK) {
    return usb_err;
  }

  // Link separate parsers to the USB handle
  gAppState.husb.Parsers[USB_MSG_STK500V2] = &STK500_Parser;

  return usb_err;
}

void CDC_CB_Connected() {
  uint8_t test = 1;
  (void)test;
}
void CDC_CB_Disconnected() {
  uint8_t test = 1;
  (void)test;
}

void CDC_CB_DataReceived(uint8_t *Buf, uint32_t *Len) {
  if (*Len <= 0)
    return;

  USB_MessageStatusTypeDef parse_status;

  parse_status = parse_cmd(Buf, Len);

  switch (parse_status) {
  case USB_MESSAGE_OK:
    break;
  case USB_MESSAGE_PARSE_ERR:
    // TODO: Add some error handling...
    break;
  default:
    break;
  }
}

USB_MessageStatusTypeDef parse_cmd(uint8_t *Buf, uint32_t *Len) {
  switch (Buf[0]) {
  case STK500V2_START_BYTE:
    return gAppState.husb.Parsers[USB_MSG_STK500V2]->ParseCmd(Buf, Len);
  default:
    break;
  }
  return USB_MESSAGE_OK;
}