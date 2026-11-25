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
/* Returns 1 when a measurement was taken; point->pressed indicates contact. */
uint8_t Touch_Read(TouchPoint *point);
/* Returns one gesture event, then TOUCH_NONE until another gesture occurs. */
TouchGesture Touch_GetGesture(void);

#endif
