/* USER CODE BEGIN Header */
/**
  * @file    main.c
  * @brief   Snake game for STM32F411CEU6 + bare 1588BS 8x8 matrix.
  *
  * Matrix wiring used by this project:
  *   Rows (common anodes): PA0..PA7
  *   Columns (cathodes):   PB3..PB9, PA15
  *   Place one 1k resistor in series with EACH of the 8 cathode/column lines.
  *
  * Joystick:
  *   VRx -> PB0 / ADC1_IN8
  *   VRy -> PB1 / ADC1_IN9
  *   SW  -> PB10 (active-low, internal pull-up)
  *
  * 1588BS is common-anode: selected row HIGH, selected column LOW.
  * Confirm the physical matrix pin-to-row/column mapping before powering it.
  */
/* USER CODE END Header */

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim2;

#define MATRIX_ROWS             8U
#define MATRIX_COLS             8U
#define JOY_LOW_THRESHOLD       1200U
#define JOY_HIGH_THRESHOLD      2900U
#define GAME_STEP_MS            230U
#define BUTTON_DEBOUNCE_MS      35U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} MatrixPin;

/* User-selected GPIO allocation. */
static const MatrixPin rowPins[8] = {
    {GPIOA, GPIO_PIN_0}, /* R1 */
    {GPIOA, GPIO_PIN_1}, /* R2 */
    {GPIOA, GPIO_PIN_2}, /* R3 */
    {GPIOA, GPIO_PIN_3}, /* R4 */
    {GPIOA, GPIO_PIN_4}, /* R5 */
    {GPIOA, GPIO_PIN_5}, /* R6 */
    {GPIOA, GPIO_PIN_6}, /* R7 */
    {GPIOA, GPIO_PIN_7}  /* R8 */
};

static const MatrixPin colPins[8] = {
    {GPIOB, GPIO_PIN_3}, /* C1 */
    {GPIOB, GPIO_PIN_4}, /* C2 */
    {GPIOB, GPIO_PIN_5}, /* C3 */
    {GPIOB, GPIO_PIN_6}, /* C4 */
    {GPIOB, GPIO_PIN_7}, /* C5 */
    {GPIOB, GPIO_PIN_8}, /* C6 */
    {GPIOB, GPIO_PIN_9}, /* C7 */
    {GPIOA, GPIO_PIN_15} /* C8 */
};

/* Bit 7 is the leftmost pixel. Written by the game, scanned by TIM2 ISR. */
static volatile uint8_t displayBuffer[8] = {0};
static volatile uint8_t scanRow = 0;

typedef enum {
    STATE_STARTUP = 0,
    STATE_MENU,
    STATE_PLAYING,
    STATE_GAME_OVER
} GameState;

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} Direction;

static GameState gameState = STATE_STARTUP;
static uint8_t snakeX[64];
static uint8_t snakeY[64];
static uint8_t snakeLength = 3;
static uint8_t foodX = 5;
static uint8_t foodY = 3;
static Direction direction = DIR_RIGHT;
static uint32_t lastGameStep = 0;
static uint32_t randomState = 0xA53C91E7U;
static uint8_t buttonLatched = 0;
static uint32_t lastButtonChange = 0;
static GPIO_PinState lastRawButton = GPIO_PIN_SET;

/* Forward declarations. */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void Matrix_SetPixel(uint8_t x, uint8_t y, uint8_t on);
static void Matrix_Clear(void);
static void Matrix_Show(const uint8_t image[8]);
static void Startup_Animation(void);
static void Show_Menu(void);
static void Start_Game(void);
static void Game_Update(void);
static void Game_Render(void);
static void Game_Over(void);
static void Joystick_Update(void);
static uint16_t ADC_ReadChannel(uint32_t channel);
static uint8_t Button_PressedEvent(void);
static uint32_t Random_Next(void);
static void Food_Spawn(void);
static uint8_t Snake_Contains(uint8_t x, uint8_t y);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_TIM2_Init();

    Matrix_Clear();
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
        Error_Handler();
    }

    Startup_Animation();
    Show_Menu();

    while (1) {
        uint32_t now = HAL_GetTick();
        uint8_t pressed = Button_PressedEvent();

        if (gameState == STATE_MENU) {
            if (pressed) {
                Start_Game();
            }
        } else if (gameState == STATE_PLAYING) {
            Joystick_Update();
            if ((now - lastGameStep) >= GAME_STEP_MS) {
                lastGameStep = now;
                Game_Update();
                if (gameState == STATE_PLAYING) {
                    Game_Render();
                }
            }
        } else if (gameState == STATE_GAME_OVER) {
            if (pressed) {
                Start_Game();
            }
        }

        HAL_Delay(2);
    }
}

/* ----------------------------- Display driver ----------------------------- */

static void Matrix_AllRowsOff(void)
{
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        HAL_GPIO_WritePin(rowPins[i].port, rowPins[i].pin, GPIO_PIN_RESET);
    }
}

static void Matrix_AllColumnsOff(void)
{
    /* Common-anode matrix: cathode HIGH means LED off. */
    for (uint8_t i = 0; i < MATRIX_COLS; i++) {
        HAL_GPIO_WritePin(colPins[i].port, colPins[i].pin, GPIO_PIN_SET);
    }
}

static void Matrix_Clear(void)
{
    for (uint8_t y = 0; y < 8; y++) {
        displayBuffer[y] = 0;
    }
}

static void Matrix_SetPixel(uint8_t x, uint8_t y, uint8_t on)
{
    if ((x >= 8U) || (y >= 8U)) return;

    uint8_t mask = (uint8_t)(1U << (7U - x));
    if (on) displayBuffer[y] |= mask;
    else    displayBuffer[y] &= (uint8_t)~mask;
}

static void Matrix_Show(const uint8_t image[8])
{
    for (uint8_t y = 0; y < 8; y++) {
        displayBuffer[y] = image[y];
    }
}

/* One row is refreshed on each TIM2 interrupt. At 1 kHz, full-frame rate=125 Hz. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2) return;

    Matrix_AllRowsOff();
    Matrix_AllColumnsOff();

    uint8_t row = scanRow;
    uint8_t pixels = displayBuffer[row];

    for (uint8_t col = 0; col < 8; col++) {
        if ((pixels & (uint8_t)(1U << (7U - col))) != 0U) {
            /* Common-anode: pull selected cathode LOW to illuminate pixel. */
            HAL_GPIO_WritePin(colPins[col].port, colPins[col].pin, GPIO_PIN_RESET);
        }
    }

    /* Enable the selected common-anode row only after setting columns. */
    HAL_GPIO_WritePin(rowPins[row].port, rowPins[row].pin, GPIO_PIN_SET);

    scanRow++;
    if (scanRow >= 8U) scanRow = 0U;
}

/* ------------------------------ UI screens -------------------------------- */

static void Startup_Animation(void)
{
    static const uint8_t circleFrames[4][8] = {
        {0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x00},
        {0x00,0x18,0x24,0x24,0x24,0x24,0x18,0x00},
        {0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C},
        {0x00,0x18,0x24,0x42,0x42,0x24,0x18,0x00}
    };

    for (uint8_t repeat = 0; repeat < 2; repeat++) {
        for (uint8_t f = 0; f < 4; f++) {
            Matrix_Show(circleFrames[f]);
            HAL_Delay(180);
        }
        for (int8_t f = 2; f >= 0; f--) {
            Matrix_Show(circleFrames[(uint8_t)f]);
            HAL_Delay(180);
        }
    }
}

static void Show_Menu(void)
{
    /* Right-pointing PLAY triangle. Press joystick SW to start. */
    static const uint8_t playIcon[8] = {
        0x00, 0x40, 0x60, 0x70, 0x78, 0x70, 0x60, 0x40
    };
    Matrix_Show(playIcon);
    gameState = STATE_MENU;
}

/* ------------------------------- Snake game ------------------------------- */

static void Start_Game(void)
{
    snakeLength = 3;
    snakeX[0] = 3; snakeY[0] = 4;
    snakeX[1] = 2; snakeY[1] = 4;
    snakeX[2] = 1; snakeY[2] = 4;
    direction = DIR_RIGHT;
    randomState ^= HAL_GetTick() + 0x9E3779B9U;
    Food_Spawn();
    lastGameStep = HAL_GetTick();
    gameState = STATE_PLAYING;
    Game_Render();
}

static void Game_Update(void)
{
    uint8_t nextX = snakeX[0];
    uint8_t nextY = snakeY[0];

    switch (direction) {
        case DIR_UP:    if (nextY == 0U) { Game_Over(); return; } nextY--; break;
        case DIR_RIGHT: if (nextX == 7U) { Game_Over(); return; } nextX++; break;
        case DIR_DOWN:  if (nextY == 7U) { Game_Over(); return; } nextY++; break;
        case DIR_LEFT:  if (nextX == 0U) { Game_Over(); return; } nextX--; break;
        default: break;
    }

    uint8_t eating = (uint8_t)((nextX == foodX) && (nextY == foodY));
    uint8_t collisionLimit = snakeLength;
    if (!eating && collisionLimit > 0U) collisionLimit--;

    for (uint8_t i = 0; i < collisionLimit; i++) {
        if ((snakeX[i] == nextX) && (snakeY[i] == nextY)) {
            Game_Over();
            return;
        }
    }

    if (eating && snakeLength < 64U) {
        snakeLength++;
    }

    for (int16_t i = (int16_t)snakeLength - 1; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
    }
    snakeX[0] = nextX;
    snakeY[0] = nextY;

    if (eating) Food_Spawn();
}

static void Game_Render(void)
{
    Matrix_Clear();

    /* Food blinks slowly using the HAL millisecond tick. */
    if ((HAL_GetTick() / 250U) & 1U) {
        Matrix_SetPixel(foodX, foodY, 1);
    }

    for (uint8_t i = 0; i < snakeLength; i++) {
        Matrix_SetPixel(snakeX[i], snakeY[i], 1);
    }
}

static void Game_Over(void)
{
    static const uint8_t gameOverIcon[8] = {
        0x81,0x42,0x24,0x18,0x18,0x24,0x42,0x81
    };
    Matrix_Show(gameOverIcon);
    gameState = STATE_GAME_OVER;
}

static void Joystick_Update(void)
{
    uint16_t x = ADC_ReadChannel(ADC_CHANNEL_8);
    uint16_t y = ADC_ReadChannel(ADC_CHANNEL_9);

    /* Change direction only when one axis clearly leaves its centre zone.
       Reversing directly into the neck is ignored. */
    Direction requested = direction;
    if (x < JOY_LOW_THRESHOLD) requested = DIR_LEFT;
    else if (x > JOY_HIGH_THRESHOLD) requested = DIR_RIGHT;
    else if (y < JOY_LOW_THRESHOLD) requested = DIR_UP;
    else if (y > JOY_HIGH_THRESHOLD) requested = DIR_DOWN;

    if ((uint8_t)((requested + 2U) & 3U) != (uint8_t)direction) {
        direction = requested;
    }
}

static uint16_t ADC_ReadChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef config = {0};
    config.Channel = channel;
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK) return 2048U;
    if (HAL_ADC_Start(&hadc1) != HAL_OK) return 2048U;

    uint16_t value = 2048U;
    if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK) {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    return value;
}

static uint8_t Button_PressedEvent(void)
{
    GPIO_PinState raw = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10);
    uint32_t now = HAL_GetTick();

    if (raw != lastRawButton) {
        lastRawButton = raw;
        lastButtonChange = now;
    }

    if ((now - lastButtonChange) >= BUTTON_DEBOUNCE_MS) {
        if ((raw == GPIO_PIN_RESET) && (buttonLatched == 0U)) {
            buttonLatched = 1U;
            return 1U;
        }
        if (raw == GPIO_PIN_SET) {
            buttonLatched = 0U;
        }
    }
    return 0U;
}

static uint32_t Random_Next(void)
{
    randomState = randomState * 1664525U + 1013904223U;
    return randomState;
}

static uint8_t Snake_Contains(uint8_t x, uint8_t y)
{
    for (uint8_t i = 0; i < snakeLength; i++) {
        if ((snakeX[i] == x) && (snakeY[i] == y)) return 1U;
    }
    return 0U;
}

static void Food_Spawn(void)
{
    if (snakeLength >= 64U) {
        Game_Over();
        return;
    }

    uint8_t attempts = 0;
    do {
        uint32_t r = Random_Next();
        foodX = (uint8_t)(r & 7U);
        foodY = (uint8_t)((r >> 8) & 7U);
        attempts++;
    } while (Snake_Contains(foodX, foodY) && attempts < 100U);
}

/* ----------------------------- CubeMX setup ------------------------------- */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void)
{
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    ADC_ChannelConfTypeDef config = {0};
    config.Channel = ADC_CHANNEL_8;
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef clockSource = {0};
    TIM_MasterConfigTypeDef master = {0};

    htim2.Instance = TIM2;
    /* HSI=16 MHz, APB1 prescaler=1 => TIM2 clock=16 MHz.
       16 MHz / (15+1) / (999+1) = 1 kHz row-scan interrupt. */
    htim2.Init.Prescaler = 15;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 999;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    clockSource.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &clockSource) != HAL_OK) Error_Handler();

    master.MasterOutputTrigger = TIM_TRGO_RESET;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &master) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Safe idle for common-anode matrix: rows LOW, cathode columns HIGH. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                      GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                      GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 |
                      GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_SET);

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
               GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
               GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 |
               GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
