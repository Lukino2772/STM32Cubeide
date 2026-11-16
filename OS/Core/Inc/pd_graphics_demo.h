#ifndef PD_GRAPHICS_DEMO_H
#define PD_GRAPHICS_DEMO_H
/* Display-independent RGB565 graphics demonstrations.
 * Portable C99, no heap, no floating point, no full framebuffer, no HAL.
 * Caller owns all display transactions, timers and a small pixel scratch.
 * Rendering callbacks must complete synchronously.
 */
#include "pd_bus.h"

#define PD_GFX_DEMO_MIN_SCRATCH_PIXELS (121U) /* one 11x11 solar tile */
#define PD_GFX_DEMO_MAX_DIMENSION (480U)
#define PD_GFX_DEMO_PLANETS (8U)
/* Fixed 11x11 dirty tiles: at most 44x44 tiles on a 480x480 panel. */
#define PD_GFX_DEMO_TILE_SIZE (11U)
#define PD_GFX_DEMO_TILE_WORDS (61U)
/* Fast-path optional caller-owned command array: 352 entries cover every
 * built-in scene, including the 320-edge torus. No heap or framebuffer. */
#define PD_GFX_DEMO_FAST_COMMANDS (352U)
typedef struct {
    int16_t x0, y0, x1, y1; /* inclusive ends; fill stores opposite corners */
    uint16_t color;
    uint8_t kind; /* 0=line, 1=filled rectangle */
} pd_gfx_command_t;

typedef struct {
    void *ctx;
    uint16_t width, height;
    pd_status_t (*fill)(void *, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height, uint16_t rgb565);
    pd_status_t (*line)(void *, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, uint16_t rgb565);
    /* Pixels are host-endian uint16_t RGB565, row-major and borrowed only
     * until the synchronous callback returns. For SPI TFT use the checked
     * RGB565 host-endian API, not the *_be byte stream API. */
    pd_status_t (*blit)(void *, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height,
                        const uint16_t *pixels, size_t pixel_count);
    /* Optional. Caller implements text using its display font facilities. */
    pd_status_t (*text)(void *, uint16_t x, uint16_t y,
                        const char *message, uint16_t fg, uint16_t bg);
    /* Optional for demos; required by pd_gfx_benchmark_run(). Wrap-safe
     * monotonically increasing unsigned milliseconds. */
    uint32_t (*millis)(void *);
} pd_gfx_display_t;

typedef enum {
    PD_GFX_SCENE_SINE = 0,
    PD_GFX_SCENE_DAMPED_WAVE,
    PD_GFX_SCENE_PARABOLA,
    PD_GFX_SCENE_EXPONENTIAL,
    PD_GFX_SCENE_LISSAJOUS,
    PD_GFX_SCENE_CUBE,
    PD_GFX_SCENE_PYRAMID,
    PD_GFX_SCENE_SPHERE,
    PD_GFX_SCENE_TORUS,
    PD_GFX_SCENE_HELIX,
    PD_GFX_SCENE_SADDLE,
    PD_GFX_SCENE_RIPPLE,
    PD_GFX_SCENE_SOLAR_SYSTEM,
    PD_GFX_SCENE_PENDULUM,
    PD_GFX_SCENE_DOUBLE_PENDULUM,
    PD_GFX_SCENE_CRANK_SLIDER,
    PD_GFX_SCENE_GEARS,
    PD_GFX_SCENE_GYROSCOPE,
    PD_GFX_SCENE_COUNT
} pd_gfx_scene_t;

typedef struct {
    pd_gfx_display_t screen;
    uint16_t *scratch;
    size_t scratch_pixels;
    uint32_t last_solar_ms;
    uint32_t orbit_phase[PD_GFX_DEMO_PLANETS];
    int16_t old_x[PD_GFX_DEMO_PLANETS], old_y[PD_GFX_DEMO_PLANETS];
    int16_t new_x[PD_GFX_DEMO_PLANETS], new_y[PD_GFX_DEMO_PLANETS];
    uint16_t solar_speed_q8; /* 256 = 1x */
    pd_gfx_scene_t previous_scene;
    bool initialized, solar_ready;
    pd_gfx_command_t *commands; /* optional external acceleration storage */
    size_t command_capacity;
    uint32_t occupied[PD_GFX_DEMO_TILE_WORDS];
    uint32_t last_geometry_hash;
    size_t last_command_count;
} pd_gfx_demo_t;

/* No allocation. Supply >=121 RGB565 pixels of caller-owned scratch.
 * Display callbacks must be valid for the object's entire lifetime.
 * Scene renders are not reentrant. Hold a full display/bus lock. */
pd_status_t pd_gfx_demo_init(pd_gfx_demo_t *demo,
                             const pd_gfx_display_t *screen,
                             uint16_t *scratch, size_t scratch_pixels);
void pd_gfx_demo_invalidate(pd_gfx_demo_t *demo);
/* Optional fast path. Supply a separate static array of >=352 commands to
 * avoid recalculating geometry per dirty tile. Smaller arrays are safe: the
 * renderer automatically falls back to a slower, low-RAM path on overflow.
 * Never overlap commands with scratch; keep storage alive across renders. */
pd_status_t pd_gfx_demo_set_command_cache(pd_gfx_demo_t *demo,
    pd_gfx_command_t *commands, size_t capacity);
pd_status_t pd_gfx_demo_set_solar_speed(pd_gfx_demo_t *demo,
                                       uint16_t speed_q8); /* 0..4096 */
const char *pd_gfx_demo_scene_name(pd_gfx_scene_t scene);
pd_status_t pd_gfx_demo_render(pd_gfx_demo_t *demo, pd_gfx_scene_t scene,
                               uint32_t now_ms);
#endif
