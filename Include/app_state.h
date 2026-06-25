#ifndef INCLUDE_APP_STATE
#define INCLUDE_APP_STATE

#include "usbd_def.h"
#include "usb.h"

typedef struct {
    USB_HandleTypeDef husb;
} APP_State;

extern APP_State gAppState;

#endif /* INCLUDE_APP_STATE */