#include "touch.h"
#include "scope.h"
#include <stdlib.h>

/*
 * Kuongshun SPFD5408 UNO-style shield, using the existing OS LCD wiring.
 * The common shield electrode assignment is XP=D8, YM=D9, XM=A2, YP=A3.
 *
 * This project maps those signals as:
 *   XP = PB0 (LCD D0)     YM = PB1 (LCD D1)
 *   XM = PA3 (LCD RS)     YP = PA4 (LCD CS)
 *
 * PA3 is ADC1_IN3 and PA4 is ADC1_IN4. No touch-controller IC or SPI bus
 * is used. The pins are time-multiplexed between LCD and touch operation.
 */
#define TOUCH_XP_PORT GPIOB
#define TOUCH_XP_PIN  GPIO_PIN_0
#define TOUCH_YM_PORT GPIOB
#define TOUCH_YM_PIN  GPIO_PIN_1
#define TOUCH_XM_PORT GPIOA
#define TOUCH_XM_PIN  GPIO_PIN_3
#define TOUCH_YP_PORT GPIOA
#define TOUCH_YP_PIN  GPIO_PIN_4

#define TOUCH_ADC_X_CHANNEL ADC_CHANNEL_4
#define TOUCH_ADC_Y_CHANNEL ADC_CHANNEL_3

/* Typical starting calibration for this shield; recalibrate on hardware. */
#define TOUCH_RAW_X_MIN  600U
#define TOUCH_RAW_X_MAX  3680U
#define TOUCH_RAW_Y_MIN  600U
#define TOUCH_RAW_Y_MAX  3680U
#define TOUCH_MIN_PRESSURE  8U
#define TOUCH_MAX_PRESSURE  1000U
#define TOUCH_X_MAX 239U
#define TOUCH_Y_MAX 319U
#define TOUCH_SWIPE_DELTA 28
#define TOUCH_LONG_MS 700U
#define TOUCH_TAP_MAX_MS 450U

extern ADC_HandleTypeDef hadc1;

static uint8_t gesture_was_pressed;
static uint8_t long_press_sent;
static uint32_t press_started;
static int16_t press_start_x;
static int16_t press_start_y;
static int16_t press_last_x;
static int16_t press_last_y;

static void PinMode(GPIO_TypeDef *port, uint16_t pin, uint32_t mode)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = mode;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &gpio);
}

static void TouchPinsToLcd(void)
{
    /* Keep LCD deselected and restore the shared bus pins to output mode. */
    PinMode(GPIOA, TOUCH_XM_PIN | TOUCH_YP_PIN, GPIO_MODE_OUTPUT_PP);
    PinMode(GPIOB, TOUCH_XP_PIN | TOUCH_YM_PIN, GPIO_MODE_OUTPUT_PP);

    HAL_GPIO_WritePin(GPIOA, TOUCH_XM_PIN | TOUCH_YP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, TOUCH_XP_PIN | TOUCH_YM_PIN, GPIO_PIN_RESET);
}

static uint16_t ADC_ReadTouch(uint32_t channel)
{
    ADC_ChannelConfTypeDef config = {0};
    config.Channel = channel;
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK)
        return 0U;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
        return 0U;

    if (HAL_ADC_PollForConversion(&hadc1, 2U) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return 0U;
    }

    uint16_t value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
    return value;
}

static uint16_t ReadAverage(uint32_t channel)
{
    uint32_t total = 0U;
    for (uint8_t i = 0U; i < 4U; ++i)
        total += ADC_ReadTouch(channel);
    return (uint16_t)(total / 4U);
}

static uint16_t ClampMap(uint16_t value, uint16_t in_min,
                         uint16_t in_max, uint16_t out_max)
{
    if (value < in_min) value = in_min;
    if (value > in_max) value = in_max;
    return (uint16_t)(((uint32_t)(value - in_min) * out_max) /
                      (uint32_t)(in_max - in_min));
}

void Touch_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gesture_was_pressed = 0U;
    long_press_sent = 0U;
    press_started = 0U;

    /* ADC1 returns to the oscilloscope's timer-triggered PA0 configuration. */
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    (void)HAL_ADC_Init(&hadc1);

    ADC_ChannelConfTypeDef config = {0};
    config.Channel = ADC_CHANNEL_0;
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    (void)HAL_ADC_ConfigChannel(&hadc1, &config);

    TouchPinsToLcd();
}

uint8_t Touch_Read(TouchPoint *point)
{
    if (point == NULL || Scope_IsAcquiring())
        return 0U;

    point->x = 0U;
    point->y = 0U;
    point->z = 0U;
    point->pressed = 0U;

    /* Pause the scope ADC trigger while the same ADC peripheral reads touch. */
    (void)HAL_ADC_Stop(&hadc1);
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
        goto restore;

    /* X coordinate: drive XP high/XM low, sample YP on ADC1_IN4. */
    PinMode(GPIOB, TOUCH_XP_PIN, GPIO_MODE_OUTPUT_PP);
    PinMode(GPIOA, TOUCH_XM_PIN, GPIO_MODE_OUTPUT_PP);
    PinMode(GPIOA, TOUCH_YP_PIN, GPIO_MODE_ANALOG);
    PinMode(GPIOB, TOUCH_YM_PIN, GPIO_MODE_ANALOG);
    HAL_GPIO_WritePin(TOUCH_XP_PORT, TOUCH_XP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TOUCH_XM_PORT, TOUCH_XM_PIN, GPIO_PIN_RESET);
    uint16_t raw_x = ReadAverage(TOUCH_ADC_X_CHANNEL);

    /* Y coordinate: drive YP high/YM low, sample XM on ADC1_IN3. */
    PinMode(GPIOA, TOUCH_YP_PIN, GPIO_MODE_OUTPUT_PP);
    PinMode(GPIOB, TOUCH_YM_PIN, GPIO_MODE_OUTPUT_PP);
    PinMode(GPIOA, TOUCH_XM_PIN, GPIO_MODE_ANALOG);
    PinMode(GPIOB, TOUCH_XP_PIN, GPIO_MODE_ANALOG);
    HAL_GPIO_WritePin(TOUCH_YP_PORT, TOUCH_YP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TOUCH_YM_PORT, TOUCH_YM_PIN, GPIO_PIN_RESET);
    uint16_t raw_y = ReadAverage(TOUCH_ADC_Y_CHANNEL);

    /* Pressure estimate: excite X plate and measure both plate voltages. */
    PinMode(GPIOB, TOUCH_XP_PIN, GPIO_MODE_OUTPUT_PP);
    PinMode(GPIOB, TOUCH_YM_PIN, GPIO_MODE_OUTPUT_PP);
    PinMode(GPIOA, TOUCH_XM_PIN, GPIO_MODE_ANALOG);
    PinMode(GPIOA, TOUCH_YP_PIN, GPIO_MODE_ANALOG);
    HAL_GPIO_WritePin(TOUCH_XP_PORT, TOUCH_XP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TOUCH_YM_PORT, TOUCH_YM_PIN, GPIO_PIN_RESET);
    uint16_t z1 = ADC_ReadTouch(TOUCH_ADC_X_CHANNEL);
    uint16_t z2 = ADC_ReadTouch(TOUCH_ADC_Y_CHANNEL);

    if (z1 > 0U && z2 >= z1)
    {
        uint16_t x10 = (uint16_t)(raw_x >> 2);
        uint16_t z1_10 = (uint16_t)(z1 >> 2);
        uint16_t z2_10 = (uint16_t)(z2 >> 2);
        uint32_t pressure = (1023U - (uint32_t)(z2_10 - z1_10));
        pressure = (pressure * x10 * 300U) >> 10;
        if (pressure > 0xFFFFU) pressure = 0xFFFFU;
        point->z = (uint16_t)pressure;
    }

    point->pressed = (point->z >= TOUCH_MIN_PRESSURE &&
                      point->z <= TOUCH_MAX_PRESSURE) ? 1U : 0U;

    if (point->pressed)
    {
        point->x = ClampMap(raw_x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX,
                            TOUCH_X_MAX);
        point->y = ClampMap(raw_y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX,
                            TOUCH_Y_MAX);
    }

restore:
    TouchPinsToLcd();

    /* Restore timer-triggered ADC1 channel 0 for the next scope capture. */
    (void)HAL_ADC_Stop(&hadc1);
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    (void)HAL_ADC_Init(&hadc1);
    ADC_ChannelConfTypeDef config0 = {0};
    config0.Channel = ADC_CHANNEL_0;
    config0.Rank = 1;
    config0.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    (void)HAL_ADC_ConfigChannel(&hadc1, &config0);

    return 1U;
}

TouchGesture Touch_GetGesture(void)
{
    TouchPoint point;
    if (!Touch_Read(&point))
        return TOUCH_NONE;

    uint32_t now = HAL_GetTick();

    if (point.pressed)
    {
        if (!gesture_was_pressed)
        {
            gesture_was_pressed = 1U;
            long_press_sent = 0U;
            press_started = now;
            press_start_x = (int16_t)point.x;
            press_start_y = (int16_t)point.y;
            press_last_x = (int16_t)point.x;
            press_last_y = (int16_t)point.y;
        }
        else
        {
            press_last_x = (int16_t)point.x;
            press_last_y = (int16_t)point.y;
        }
        if (!long_press_sent && (now - press_started) >= TOUCH_LONG_MS)
        {
            long_press_sent = 1U;
            return TOUCH_LONG_PRESS;
        }
        return TOUCH_NONE;
    }

    if (!gesture_was_pressed)
        return TOUCH_NONE;

    gesture_was_pressed = 0U;
    if (long_press_sent)
        return TOUCH_NONE;

    int16_t dx = press_last_x - press_start_x;
    int16_t dy = press_last_y - press_start_y;
    uint32_t duration = now - press_started;

    if (abs(dx) >= TOUCH_SWIPE_DELTA &&
        abs(dx) >= abs(dy))
        return (dx < 0) ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;

    if (abs(dy) >= TOUCH_SWIPE_DELTA)
        return (dy < 0) ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN;

    if (duration <= TOUCH_TAP_MAX_MS)
        return TOUCH_TAP;

    return TOUCH_NONE;
}
