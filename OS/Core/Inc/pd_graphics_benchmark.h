#ifndef PD_GRAPHICS_BENCHMARK_H
#define PD_GRAPHICS_BENCHMARK_H
#include "pd_graphics_demo.h"

/* Display benchmark is blocking. Times measure completed display callbacks
 * and any synchronous transport/DMA wait, NOT only CPU rendering speed.
 * The results page is drawn after timing stops and is excluded.
 * A working millisecond timebase and text callback are required. */
#define PD_GFX_BENCH_STAGE_COUNT (6U)
typedef enum {
    PD_GFX_BENCH_FILLS = 0,
    PD_GFX_BENCH_CHECKERS,
    PD_GFX_BENCH_COLOR_BARS,
    PD_GFX_BENCH_LINES,
    PD_GFX_BENCH_RECTANGLES,
    PD_GFX_BENCH_PIXELS
} pd_gfx_bench_stage_t;
typedef struct {
    uint32_t stage_ms[PD_GFX_BENCH_STAGE_COUNT];
    uint32_t total_ms;
    uint32_t fill_calls;
    uint32_t line_calls;
    uint32_t addressed_pixels; /* includes overdraw; not unique pixels */
} pd_gfx_bench_result_t;

const char *pd_gfx_benchmark_stage_name(pd_gfx_bench_stage_t stage);
pd_status_t pd_gfx_benchmark_run(const pd_gfx_display_t *screen,
                                 pd_gfx_bench_result_t *result);
#endif
