#ifndef INCLUDE_USB
#define INCLUDE_USB

#include "usbd_def.h"
#include <stdint.h>

#define USB_MESSAGE_PARSERS_COUNT       1

// Should be sufficient for both stk500v2 and ATMEL ISP
#define USB_TX_SIZE                     600
#define USB_RX_SIZE                     600

typedef enum {
    USB_MSG_STK500V2,
    USB_MSG_TYPE_COUNT                  // total number of types
} USB_ActiveMessageTypeDef;
typedef enum {
    USB_MESSAGE_OK,
    USB_MESSAGE_PARSE_ERR
} USB_MessageStatusTypeDef;
typedef struct {
    USB_MessageStatusTypeDef(*ParseCmd)(uint8_t *Buf, uint32_t *Len);
} USB_MessageParserTypeDef;
typedef struct {
    // Each parser is placed according to its message type index from USB_ActiveMessageTypeDef
    USB_MessageParserTypeDef *Parsers[USB_MESSAGE_PARSERS_COUNT];
    uint8_t RxBuffer[USB_RX_SIZE];
    uint8_t TxBuffer[USB_TX_SIZE];
    USBD_HandleTypeDef husbd;
} USB_HandleTypeDef;

_Static_assert(USB_MESSAGE_PARSERS_COUNT == USB_MSG_TYPE_COUNT,
               "USB_MESSAGE_PARSERS_COUNT does not match message type count");

USBD_StatusTypeDef USB_Init();

extern uint8_t buffer[1024];

#endif /* INCLUDE_USB */
