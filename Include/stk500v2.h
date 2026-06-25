#ifndef INCLUDE_STK500V2
#define INCLUDE_STK500V2

#include "usb.h"

#define MAX_BODY_SIZE                   275
#define STK500V2_START_BYTE             0x1B

typedef struct __attribute__((packed)) {
    uint8_t MessageStart;
    uint8_t SequenceNumber;
    uint8_t MessageSize[2];
    uint8_t Token;
    uint8_t MessageBody[MAX_BODY_SIZE];
    uint8_t Checksum;
} STK500V2_MessageTypeDef;

extern USB_MessageParserTypeDef STK500_Parser;

#endif /* INCLUDE_STK500V2 */
