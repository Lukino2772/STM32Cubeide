# STM32F411CEU6 graphics demo integration

The OS project now uses the current portable graphics code from
[Kutmen69/Peripheral-drivers/graphics](https://github.com/Kutmen69/Peripheral-drivers/tree/main/graphics).

## Hardware adapter

- MCU: STM32F411CEU6 Black Pill.
- Display: existing 240x320 8-bit parallel TFT implementation in
  `Core/Src/lcd.c`.
- The existing LCD driver detects ILI9325/ILI9328 or ILI9341 and owns all
  GPIO mapping, reset, controller initialization and RGB565 writes.
- Graphics use `LCD_FillRectangle()`, `LCD_DrawPixel()` and the synchronous
  host-endian `LCD_BlitRGB565()` callback. No replacement LCD driver,
  new pin mapping, SPI, DMA or CubeMX peripheral configuration is required.

## Imported portable files

`Core/Inc`:
- `pd_bus.h`
- `pd_render3d.h`
- `pd_graphics_demo.h`
- `pd_graphics_demo_internal.h`
- `pd_graphics_benchmark.h`

`Core/Src`:
- `pd_render3d.c`
- `pd_graphics_demo.c`
- `pd_graphics_solar.c`
- `pd_graphics_benchmark.c`

The application adapter is `graphics_test.c/.h`. It connects the portable
callbacks to the already initialized project LCD and HAL tick.

## Memory and performance

- 121 RGB565 scratch pixels: 242 bytes.
- 352 cached render commands: roughly 4.2 KiB (compiler-dependent).
- Demo state and benchmark result structure.
- No full-screen framebuffer, heap allocation, floating-point math or
  additional MCU peripherals.

The animated scenes use the library's 11x11 dirty-tile renderer and blit
callback. The line callback is used for static grid lines and benchmark
line tests; diagonal lines use a simple Bresenham reference implementation
and may be slow. Benchmark timings include the real LCD callback overhead.

## Startup

In `main.c`, `RUN_GRAPHICS_TEST 1U` runs all 18 scenes in sequence
(about 1.2 seconds each, actual time depends on LCD speed), then executes
the six-stage benchmark. The benchmark result screen remains visible.

Set `RUN_GRAPHICS_TEST 0U` to start the original oscilloscope instead.
The existing `LCD_Init()` call must remain before `GraphicsTest_Run()`.

## Build and validation

The new C files are in the existing `Core/Src` and headers in `Core/Inc`.
Refresh the project in STM32CubeIDE, run **Project → Clean**, then build.

**Not yet verified:** no STM32CubeIDE build or physical STM32F411/TFT run
has been performed remotely. If compilation fails, send the first complete
error and its source file/line. After a successful build, verify the LCD
color output before relying on benchmark numbers.
