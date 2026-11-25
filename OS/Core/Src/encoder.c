#include "encoder.h"
#include "main.h"

/*
 * EC11-style mechanical rotary encoder wiring:
 *   CLK -> PB6
 *   DT  -> PB7
 *   SW  -> PB11
 *   GND -> GND
 *
 * All three signal pins use the STM32 internal pull-up resistors.
 */

#define ENC_GPIO       GPIOB
#define ENC_CLK_PIN    GPIO_PIN_6
#define ENC_DT_PIN     GPIO_PIN_7
#define ENC_SW_PIN     GPIO_PIN_11

#define ENC_DEBOUNCE_MS  20U
#define ENC_LONG_MS      800U

static uint8_t previous_ab;
static int8_t quarter_steps;
static uint8_t button_raw_previous = 1U;
static uint8_t button_stable = 1U;
static uint32_t button_change_tick;
static uint32_t button_press_tick;
static uint8_t long_event_sent;

static uint8_t ReadAB(void)
{
    uint8_t a = (HAL_GPIO_ReadPin(ENC_GPIO, ENC_CLK_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    uint8_t b = (HAL_GPIO_ReadPin(ENC_GPIO, ENC_DT_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    return (uint8_t)((a << 1) | b);
}

void Encoder_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = ENC_CLK_PIN | ENC_DT_PIN | ENC_SW_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ENC_GPIO, &GPIO_InitStruct);

    previous_ab = ReadAB();
    button_raw_previous =
        (HAL_GPIO_ReadPin(ENC_GPIO, ENC_SW_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    button_stable = button_raw_previous;
    button_change_tick = HAL_GetTick();
}

int8_t Encoder_GetDelta(void)
{
    /*
     * Gray-code transition decoder. Invalid two-bit jumps are ignored.
     * Four valid transitions equal one encoder detent.
     */
    static const int8_t transition_table[16] =
    {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };

    uint8_t current_ab = ReadAB();
    uint8_t index = (uint8_t)((previous_ab << 2) | current_ab);
    int8_t movement = transition_table[index];

    previous_ab = current_ab;
    quarter_steps += movement;

    if (quarter_steps >= 4)
    {
        quarter_steps = 0;
        return 1;
    }
    if (quarter_steps <= -4)
    {
        quarter_steps = 0;
        return -1;
    }

    return 0;
}

EncoderButtonEvent Encoder_GetButtonEvent(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t raw =
        (HAL_GPIO_ReadPin(ENC_GPIO, ENC_SW_PIN) == GPIO_PIN_SET) ? 1U : 0U;

    if (raw != button_raw_previous)
    {
        button_raw_previous = raw;
        button_change_tick = now;
    }

    if ((now - button_change_tick) >= ENC_DEBOUNCE_MS &&
        raw != button_stable)
    {
        button_stable = raw;

        if (button_stable == 0U)
        {
            button_press_tick = now;
            long_event_sent = 0U;
        }
        else if (long_event_sent == 0U)
        {
            return ENCODER_BUTTON_SHORT;
        }
    }

    if (button_stable == 0U && long_event_sent == 0U &&
        (now - button_press_tick) >= ENC_LONG_MS)
    {
        long_event_sent = 1U;
        return ENCODER_BUTTON_LONG;
    }

    return ENCODER_BUTTON_NONE;
}
