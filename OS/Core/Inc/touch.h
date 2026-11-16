#ifndef TOUCH_H
#define TOUCH_H

#include "main.h"
#include <stdint.h>

typedef enum
{
    TOUCH_NONE = 0,
    TOUCH_SWIPE_LEFT,
    TOUCH_SWIPE_RIGHT,
    TOUCH_SWIPE_UP,
    TOUCH_SWIPE_DOWN,
    TOUCH_TAP,
    TOUCH_LONG_PRESS
} TouchGesture;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint8_t pressed;
} TouchPoint;

void Touch_Init(void);
uint8_t Touch_Read(TouchPoint *point);
TouchGesture Touch_GetGesture(void);

#endif
