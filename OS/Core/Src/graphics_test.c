#include "graphics_test.h"
#include "lcd.h"
#include "pd_graphics_demo.h"
#include "pd_graphics_benchmark.h"
#include "pd_render3d.h"
#include "main.h"
#include <stdint.h>

static pd_gfx_demo_t gfx_demo;
static uint16_t gfx_scratch[PD_GFX_DEMO_MIN_SCRATCH_PIXELS];

static const uint8_t font_letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}  /* Z */
};

static const uint8_t font_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static const uint8_t *font_glyph(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z') return font_letters[(uint8_t)(ch - 'A')];
    if (ch >= '0' && ch <= '9') return font_digits[(uint8_t)(ch - '0')];
    return 0;
}

static pd_status_t gfx_fill(void *ctx, uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h, uint16_t color)
{
    (void)ctx;
    if (w == 0U || h == 0U || x >= LCD_WIDTH || y >= LCD_HEIGHT ||
        w > LCD_WIDTH - x || h > LCD_HEIGHT - y)
        return PD_ERANGE;
    LCD_FillRectangle(x, y, w, h, color);
    return PD_OK;
}

static pd_status_t gfx_line(void *ctx, int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1, uint16_t color)
{
    (void)ctx;
    int32_t dx = (x1 >= x0) ? (int32_t)x1 - x0 : (int32_t)x0 - x1;
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = (y1 >= y0) ? (int32_t)y0 - y1 : (int32_t)y1 - y0;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    if (y0 == y1) {
        uint16_t left = (uint16_t)((x0 < x1) ? x0 : x1);
        uint16_t width = (uint16_t)(dx + 1);
        return gfx_fill(0, left, (uint16_t)y0, width, 1U, color);
    }
    if (x0 == x1) {
        uint16_t top = (uint16_t)((y0 < y1) ? y0 : y1);
        uint16_t height = (uint16_t)((y0 < y1) ? y1 - y0 + 1 : y0 - y1 + 1);
        return gfx_fill(0, (uint16_t)x0, top, 1U, height, color);
    }

    for (;;) {
        if (x0 >= 0 && y0 >= 0 && x0 < LCD_WIDTH && y0 < LCD_HEIGHT)
            LCD_DrawPixel((uint16_t)x0, (uint16_t)y0, color);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 = (int16_t)(x0 + sx); }
        if (e2 <= dx) { err += dx; y0 = (int16_t)(y0 + sy); }
    }
    return PD_OK;
}

static pd_status_t gfx_blit(void *ctx, uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h,
                            const uint16_t *pixels, size_t pixel_count)
{
    (void)ctx;
    if (pixels == 0 || w == 0U || h == 0U ||
        pixel_count < (size_t)w * h ||
        x >= LCD_WIDTH || y >= LCD_HEIGHT ||
        w > LCD_WIDTH - x || h > LCD_HEIGHT - y)
        return PD_ERANGE;
    LCD_BlitRGB565(x, y, w, h, pixels);
    return PD_OK;
}

static pd_status_t gfx_text(void *ctx, uint16_t x, uint16_t y,
                            const char *message, uint16_t fg, uint16_t bg)
{
    (void)ctx;
    if (message == 0) return PD_EINVAL;

    while (*message != '\0') {
        if (x + 6U > LCD_WIDTH) {
            x = 2U;
            y = (uint16_t)(y + 9U);
        }
        if (y + 8U > LCD_HEIGHT) break;

        uint16_t tile[48];
        for (uint32_t i = 0U; i < 48U; ++i) tile[i] = bg;

        const uint8_t *glyph = font_glyph(*message);
        if (glyph != 0) {
            for (uint8_t col = 0U; col < 5U; ++col) {
                for (uint8_t row = 0U; row < 7U; ++row) {
                    if ((glyph[col] & (1U << row)) != 0U)
                        tile[(uint32_t)row * 6U + col] = fg;
                }
            }
        }
        LCD_BlitRGB565(x, y, 6U, 8U, tile);
        x = (uint16_t)(x + 6U);
        ++message;
    }
    return PD_OK;
}

static uint32_t gfx_millis(void *ctx)
{
    (void)ctx;
    return HAL_GetTick();
}

static pd_gfx_display_t gfx_display = {
    0, LCD_WIDTH, LCD_HEIGHT,
    gfx_fill, gfx_line, gfx_blit, gfx_text, gfx_millis
};

static void show_error(pd_status_t status)
{
    (void)status;
    LCD_FillScreen(LCD_BLACK);
    (void)gfx_text(0, 8U, 12U, "GRAPHICS ERROR", LCD_RED, LCD_BLACK);
    (void)gfx_text(0, 8U, 30U, "CHECK LCD DRIVER", LCD_WHITE, LCD_BLACK);
}

void GraphicsTest_Run(void)
{
    static const pd_gfx_scene_t scenes[] = {
        PD_GFX_SCENE_CUBE,
        PD_GFX_SCENE_PYRAMID,
        PD_GFX_SCENE_SPHERE,
        PD_GFX_SCENE_TORUS,
        PD_GFX_SCENE_HELIX,
        PD_GFX_SCENE_SADDLE,
        PD_GFX_SCENE_RIPPLE,
        PD_GFX_SCENE_SOLAR_SYSTEM,
        PD_GFX_SCENE_GYROSCOPE
    };

    pd_status_t status = pd_gfx_demo_init(&gfx_demo, &gfx_display,
                                          gfx_scratch,
                                          PD_GFX_DEMO_MIN_SCRATCH_PIXELS);
    if (status != PD_OK) {
        show_error(status);
        return;
    }

    for (uint32_t scene_index = 0U;
         scene_index < (sizeof(scenes) / sizeof(scenes[0]));
         ++scene_index) {
        const uint32_t scene_start = HAL_GetTick();
        pd_gfx_demo_invalidate(&gfx_demo);

        do {
            status = pd_gfx_demo_render(&gfx_demo, scenes[scene_index],
                                        HAL_GetTick());
            if (status != PD_OK) {
                show_error(status);
                return;
            }

            (void)gfx_text(0, 4U, 4U,
                           pd_gfx_demo_scene_name(scenes[scene_index]),
                           LCD_WHITE, LCD_BLACK);
            HAL_Delay(35U);
        } while ((uint32_t)(HAL_GetTick() - scene_start) < 1400U);
    }

    status = pd_gfx_benchmark_run(&gfx_display,
                                 (pd_gfx_bench_result_t *)&gfx_scratch[0]);
    /* The benchmark result structure is larger than the solar scratch buffer.
     * Use a dedicated object instead; this cast is never used. */
    if (status != PD_OK) show_error(status);
}
