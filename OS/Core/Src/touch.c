#include "touch.h"

/*
 * This shield uses a 4-wire resistive panel. The current STM32 wiring does
 * not identify the four panel electrodes, so an XPT2046 SPI driver is not
 * valid for this hardware. Keep the API available but do not drive unrelated
 * GPIO pins until the panel's X+/X-/Y+/Y- connections are mapped.
 *
 * Once the four electrode pins are confirmed, implement the two alternating
 * ADC/GPIO measurement phases and calibrate the raw coordinate ranges.
 */

void Touch_Init(void)
{
    /* Intentionally no pin configuration until the panel wiring is known. */
}

uint8_t Touch_Read(TouchPoint *point)
{
    if (point != NULL)
    {
        point->x = 0U;
        point->y = 0U;
        point->z = 0U;
        point->pressed = 0U;
    }
    return 0U;
}

TouchGesture Touch_GetGesture(void)
{
    return TOUCH_NONE;
}
