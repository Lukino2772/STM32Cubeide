/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : GPS speedometer for STM32F103C8T6 and 3641AS display.
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"
#include <stdlib.h>
#include <string.h>

UART_HandleTypeDef huart1;

#define DISPLAY_GPIO_PORT       GPIOA
#define DISPLAY_GPIO_PINS       (GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | \
                                 GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8)
#define SEG_A                   (1U << 0)
#define SEG_B                   (1U << 1)
#define SEG_C                   (1U << 2)
#define SEG_D                   (1U << 3)
#define SEG_E                   (1U << 4)
#define SEG_F                   (1U << 5)
#define SEG_G                   (1U << 6)
#define SEG_DP                  (1U << 7)
#define ALL_SEGMENTS            0x7FU
#define GPS_LINE_LENGTH         128U
#define GPS_TIMEOUT_MS          3000U
#define SEARCH_STEP_MS          450U
#define LOST_BLINK_MS           500U
#define SPEED_FLASH_MS          240U

static uint8_t gps_rx_char;
static char gps_rx_buffer[GPS_LINE_LENGTH];
static volatile char gps_complete_line[GPS_LINE_LENGTH];
static volatile uint8_t gps_line_ready = 0;
static uint8_t gps_rx_index = 0;

static volatile uint8_t gps_fix_valid = 0;
static volatile uint16_t gps_speed_kmh = 0;
static volatile uint32_t gps_last_valid_tick = 0;
static uint8_t gps_ever_fixed = 0;

static const uint8_t digit_segments[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,             /* 0 */
    SEG_B | SEG_C,                                             /* 1 */
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                     /* 2 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                     /* 3 */
    SEG_B | SEG_C | SEG_F | SEG_G,                             /* 4 */
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                     /* 5 */
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,             /* 6 */
    SEG_A | SEG_B | SEG_C,                                     /* 7 */
    ALL_SEGMENTS,                                              /* 8 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G              /* 9 */
};

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void Display_WriteSegments(uint8_t segments);
static void Display_ShowDigit(uint8_t digit);
static void Display_RunIntro(void);
static void GPS_ProcessLine(char *line);
static void GPS_ProcessPendingLine(void);
static void Application_UpdateDisplay(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    /* Start one-byte interrupt reception before the intro animation. */
    if (HAL_UART_Receive_IT(&huart1, &gps_rx_char, 1U) != HAL_OK) {
        Error_Handler();
    }

    Display_RunIntro();

    while (1) {
        GPS_ProcessPendingLine();

        uint32_t now = HAL_GetTick();
        if (gps_fix_valid && ((uint32_t)(now - gps_last_valid_tick) > GPS_TIMEOUT_MS)) {
            gps_fix_valid = 0;
        }

        Application_UpdateDisplay();
    }
}

static void Display_WriteSegments(uint8_t segments)
{
    /* PA1=A ... PA7=G, PA8=DP. Common cathodes are permanently at GND. */
    uint16_t output = (uint16_t)(((uint16_t)segments << 1) & DISPLAY_GPIO_PINS);
    HAL_GPIO_WritePin(DISPLAY_GPIO_PORT, DISPLAY_GPIO_PINS, GPIO_PIN_RESET);
    if (output != 0U) {
        HAL_GPIO_WritePin(DISPLAY_GPIO_PORT, output, GPIO_PIN_SET);
    }
}

static void Display_ShowDigit(uint8_t digit)
{
    if (digit > 9U) {
        Display_WriteSegments(0U);
        return;
    }
    Display_WriteSegments(digit_segments[digit]);
}

static void Display_RunIntro(void)
{
    /*
     * Whole-display circle animation:
     * Turn on all six outer segments together (the digit "0") on every
     * display position, then pulse the complete ring as one animation.
     * No individual segment chase is used.
     *
     * Since all four common cathodes are tied to GND, the same ring appears
     * on all four digits simultaneously.
     */
    const uint8_t circle = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;

    for (uint8_t pulse = 0; pulse < 3U; ++pulse) {
        Display_WriteSegments(circle);
        HAL_Delay(260U);
        Display_WriteSegments(0U);
        HAL_Delay(140U);
    }

    /* Leave the complete circle visible briefly before GPS searching. */
    Display_WriteSegments(circle);
    HAL_Delay(500U);
    Display_WriteSegments(0U);
}

static void GPS_ProcessLine(char *line)
{
    char *fields[10];
    uint8_t field_count = 1U;

    if ((strncmp(line, "$GPRMC", 6U) != 0) &&
        (strncmp(line, "$GNRMC", 6U) != 0)) {
        return;
    }

    /* Split manually so empty NMEA fields keep their correct field numbers. */
    fields[0] = line;
    for (char *p = line; (*p != '\0') && (field_count < 10U); ++p) {
        if (*p == ',') {
            *p = '\0';
            fields[field_count++] = p + 1;
        }
    }

    /* RMC fields: 0=type, 1=time, 2=status, 7=speed in knots. */
    if ((field_count < 8U) || (fields[2][0] != 'A')) {
        gps_fix_valid = 0;
        return;
    }

    float knots = (float)atof(fields[7]);
    float kmh = knots * 1.852f;
    if (kmh < 0.0f) {
        kmh = 0.0f;
    }
    if (kmh > 999.0f) {
        kmh = 999.0f;
    }

    gps_speed_kmh = (uint16_t)(kmh + 0.5f);
    gps_last_valid_tick = HAL_GetTick();
    gps_fix_valid = 1;
    gps_ever_fixed = 1;
}

static void GPS_ProcessPendingLine(void)
{
    char local_line[GPS_LINE_LENGTH];
    uint8_t have_line = 0;

    __disable_irq();
    if (gps_line_ready) {
        for (uint16_t i = 0; i < GPS_LINE_LENGTH; ++i) {
            local_line[i] = gps_complete_line[i];
            if (local_line[i] == '\0') {
                break;
            }
        }
        gps_line_ready = 0;
        have_line = 1;
    }
    __enable_irq();

    if (have_line) {
        local_line[GPS_LINE_LENGTH - 1U] = '\0';
        GPS_ProcessLine(local_line);
    }
}

static void Application_UpdateDisplay(void)
{
    static uint32_t last_animation_tick = 0;
    static uint32_t flash_started_tick = 0;
    static uint16_t previous_speed = 0xFFFFU;
    static uint8_t search_circle_visible = 0;
    static uint8_t lost_visible = 0;

    const uint32_t now = HAL_GetTick();

    if (gps_fix_valid) {
        uint16_t speed = gps_speed_kmh;

        if (speed != previous_speed) {
            previous_speed = speed;
            flash_started_tick = now;
        }

        if ((uint32_t)(now - flash_started_tick) < SPEED_FLASH_MS) {
            /* Briefly alternate the full display and the speed digit. */
            if (((now / 60U) & 1U) == 0U) {
                Display_WriteSegments(ALL_SEGMENTS);
            } else {
                Display_ShowDigit((uint8_t)(speed % 10U));
            }
        } else {
            /* With all four commons tied together, only one repeated digit is possible. */
            Display_ShowDigit((uint8_t)(speed % 10U));
        }
        return;
    }

    if (!gps_ever_fixed) {
        /*
         * GPS searching animation: pulse the complete outer ring together.
         * Do not chase individual segments.
         */
        const uint8_t circle = SEG_A | SEG_B | SEG_C |
                               SEG_D | SEG_E | SEG_F;

        if ((uint32_t)(now - last_animation_tick) >= SEARCH_STEP_MS) {
            last_animation_tick = now;
            search_circle_visible = (uint8_t)!search_circle_visible;
            Display_WriteSegments(search_circle_visible ? circle : 0U);
        }
        return;
    }

    /* GPS was previously valid but is now lost: blink 0000 (same zero on each digit). */
    if ((uint32_t)(now - last_animation_tick) >= LOST_BLINK_MS) {
        last_animation_tick = now;
        lost_visible = (uint8_t)!lost_visible;
        Display_ShowDigit(lost_visible ? 0U : 0xFFU);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        char received = (char)gps_rx_char;

        if (received == '\n') {
            gps_rx_buffer[gps_rx_index] = '\0';
            if (!gps_line_ready) {
                uint16_t i = 0;
                while ((i < GPS_LINE_LENGTH) && (gps_rx_buffer[i] != '\0')) {
                    gps_complete_line[i] = gps_rx_buffer[i];
                    ++i;
                }
                gps_complete_line[i] = '\0';
                gps_line_ready = 1;
            }
            gps_rx_index = 0;
        } else if (received != '\r') {
            if (gps_rx_index < (GPS_LINE_LENGTH - 1U)) {
                gps_rx_buffer[gps_rx_index++] = received;
            } else {
                gps_rx_index = 0;
            }
        }

        (void)HAL_UART_Receive_IT(&huart1, &gps_rx_char, 1U);
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(DISPLAY_GPIO_PORT, DISPLAY_GPIO_PINS, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = DISPLAY_GPIO_PINS;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DISPLAY_GPIO_PORT, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* Stop here if a peripheral or clock initialization fails. */
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
