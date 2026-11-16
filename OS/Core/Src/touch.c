#include "touch.h"
#include <stdlib.h>
#include <stddef.h>

extern SPI_HandleTypeDef hspi2;

#define TOUCH_CS_PORT   GPIOB
#define TOUCH_CS_PIN    GPIO_PIN_6
#define TOUCH_IRQ_PORT  GPIOB
#define TOUCH_IRQ_PIN   GPIO_PIN_7

#define TOUCH_CS_LOW()  HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_RESET)
#define TOUCH_CS_HIGH() HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET)

#define TOUCH_WIDTH   240U
#define TOUCH_HEIGHT  320U

/*
 * Initial calibration limits only. Calibrate on the real shield before
 * relying on absolute positions. Some shields need X/Y swapped or inverted.
 */
#define TOUCH_RAW_X_MIN  250
#define TOUCH_RAW_X_MAX  3800
#define TOUCH_RAW_Y_MIN  250
#define TOUCH_RAW_Y_MAX  3800

#define TOUCH_PRESSURE_MIN  50U
#define TOUCH_SWIPE_PIXELS  35
#define TOUCH_LONG_PRESS_MS 800U
#define TOUCH_TAP_MAX_MS    350U

static uint16_t XPT2046_Read12(uint8_t command)
{
    uint8_t tx[3] = { command, 0x00, 0x00 };
    uint8_t rx[3] = { 0, 0, 0 };

    TOUCH_CS_LOW();
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3U, 20U);
    TOUCH_CS_HIGH();

    if (status != HAL_OK)
        return 0U;

    return (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3) & 0x0FFFU;
}

static uint16_t XPT2046_ReadAverage(uint8_t command)
{
    uint32_t total = 0U;
    for (uint8_t i = 0U; i < 5U; ++i)
        total += XPT2046_Read12(command);
    return (uint16_t)(total / 5U);
}

static uint16_t ClampMap(int32_t value, int32_t min_value,
                        int32_t max_value, uint16_t output_max)
{
    if (max_value <= min_value)
        return 0U;

    if (value < min_value)
        value = min_value;
    if (value > max_value)
        value = max_value;

    return (uint16_t)(((value - min_value) * output_max) /
                      (max_value - min_value));
}

void Touch_Init(void)
{
    TOUCH_CS_HIGH();
}

uint8_t Touch_Read(TouchPoint *point)
{
    if (point == NULL)
        return 0U;

    if (HAL_GPIO_ReadPin(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN) == GPIO_PIN_SET)
    {
        point->pressed = 0U;
        return 0U;
    }

    uint16_t raw_x = XPT2046_ReadAverage(0xD0U);
    uint16_t raw_y = XPT2046_ReadAverage(0x90U);
    uint16_t z1 = XPT2046_Read12(0xB0U);
    uint16_t z2 = XPT2046_Read12(0xC0U);

    /*
     * A pressed resistive panel normally has Z1 > 0 and Z2 > Z1.
     * This is a presence filter, not a calibrated force measurement.
     */
    if (z1 < 10U || z2 <= z1)
    {
        point->pressed = 0U;
        return 0U;
    }

    uint16_t pressure = (uint16_t)(z1 + 4095U - z2);
    if (pressure < TOUCH_PRESSURE_MIN)
    {
        point->pressed = 0U;
        return 0U;
    }

    point->x = ClampMap(raw_x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX,
                        TOUCH_WIDTH - 1U);
    point->y = ClampMap(raw_y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX,
                        TOUCH_HEIGHT - 1U);
    point->z = pressure;
    point->pressed = 1U;
    return 1U;
}

TouchGesture Touch_GetGesture(void)
{
    static TouchPoint point;
    static uint8_t tracking = 0U;
    static uint8_t long_sent = 0U;
    static uint16_t start_x = 0U;
    static uint16_t start_y = 0U;
    static uint32_t start_tick = 0U;

    if (Touch_Read(&point))
    {
        if (!tracking)
        {
            tracking = 1U;
            long_sent = 0U;
            start_x = point.x;
            start_y = point.y;
            start_tick = HAL_GetTick();
            return TOUCH_NONE;
        }

        if (!long_sent &&
            (HAL_GetTick() - start_tick) >= TOUCH_LONG_PRESS_MS)
        {
            long_sent = 1U;
            return TOUCH_LONG_PRESS;
        }

        return TOUCH_NONE;
    }

    if (!tracking)
        return TOUCH_NONE;

    tracking = 0U;

    if (long_sent)
        return TOUCH_NONE;

    int16_t dx = (int16_t)point.x - (int16_t)start_x;
    int16_t dy = (int16_t)point.y - (int16_t)start_y;
    uint32_t duration = HAL_GetTick() - start_tick;

    if (abs(dx) > abs(dy) && abs(dx) >= TOUCH_SWIPE_PIXELS)
        return (dx < 0) ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;

    if (abs(dy) >= TOUCH_SWIPE_PIXELS)
        return (dy < 0) ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN;

    if (duration <= TOUCH_TAP_MAX_MS)
        return TOUCH_TAP;

    return TOUCH_NONE;
}
