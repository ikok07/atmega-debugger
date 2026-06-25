#ifndef USB_INCLUDE_USBD_CDC_CB
#define USB_INCLUDE_USBD_CDC_CB

#include "stm32f4xx_hal.h"

/* ------ Generic USB callbacks ------ */
__weak void CDC_CB_Connected();
__weak void CDC_CB_Disconnected();

/* ------ Data callbacks ------ */
__weak void CDC_CB_DataTxCplt(uint8_t* Buf, uint32_t *Len);
__weak void CDC_CB_DataReceived(uint8_t* Buf, uint32_t *Len);

#endif /* USB_INCLUDE_USBD_CDC_CB */
