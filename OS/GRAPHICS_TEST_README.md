# OS graphics demo and LCD benchmark

This branch adds a hardware-adapted integration of the portable graphics demo
from [Kutmen69/Peripheral-drivers](https://github.com/Kutmen69/Peripheral-drivers/tree/main/graphics).
The renderer and demo source files retain their upstream API. The board adapter
uses this project's existing STM32F411CEU6 8-bit parallel LCD driver and pin map.

## Startup selection

In `Core/Src/main.c`:

```c
#define RUN_GRAPHICS_TEST 1U
```

- `1`: run the 3D scenes, then the six-stage display benchmark. The benchmark
  result screen remains visible.
- `0`: run the original oscilloscope application (`Scope_Init/Scope_Update`).

The test sequence displays the cube, pyramid, sphere, torus, helix, saddle,
ripple, solar system and gyroscope. Each scene runs for approximately 1.4 s;
actual duration depends on LCD transfer speed. It then benchmarks full-screen
fills, checkerboard rectangles, color bars, lines, nested rectangles and
individual pixels.

## LCD adapter

`lcd.c` adds `LCD_BlitRGB565()`, a synchronous RGB565 block write. It reuses
the existing LCD window and 8-bit parallel bus implementation. The graphics
adapter uses the existing `LCD_FillRectangle()` and `LCD_DrawPixel()`
functions; it does not initialize or remap any GPIO pins itself.

The small 5x7 text renderer is included because the benchmark requires a text
callback. The demo uses a 121-pixel scratch buffer (242 bytes); there is no
full-screen framebuffer or heap allocation.

## Build and hardware test

1. Checkout this branch and import/refresh the `OS` project in STM32CubeIDE.
2. Build the Debug configuration.
3. Flash the STM32F411CEU6 and check that the LCD shows the scenes and then
   the benchmark results.
4. Record the six stage times and total time shown on the LCD.
5. Set `RUN_GRAPHICS_TEST` to `0` to return to the oscilloscope.

This repository integration has not been compiled or run on the physical board
from this environment. The timings are deliberately measured on the target:
they include synchronous LCD transfers and callback overhead, not just CPU
rendering time. A 1 ms HAL tick can report 0 ms for a very short stage.

The upstream repository describes these modules as pre-release and not
production-validated. Treat the result as a display/graphics experiment, not
as a safety-qualified component.
