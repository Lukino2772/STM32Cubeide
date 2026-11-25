# GPS Speedometer 2 — STM32F103C8T6

Firmware target: STM32F103C8T6 (Blue Pill), NEO-6M GPS, 3641AS common-cathode 4-digit display.

## Pin assignment

### 3641AS display

| Display segment | Display physical pin | STM32 pin |
|---|---:|---|
| A | 11 | PA1 |
| B | 7 | PA2 |
| C | 4 | PA3 |
| D | 2 | PA4 |
| E | 1 | PA5 |
| F | 10 | PA6 |
| G | 5 | PA7 |
| DP (optional) | 3 | PA8 |
| Common cathodes | 12, 9, 8, 6 | GND |

Use one current-limiting resistor per segment. This firmware assumes a common-cathode display (segment ON = GPIO HIGH).

**Display limitation:** all four common cathodes are tied directly to GND, so all four positions show the same segment pattern. The current firmware therefore shows the ones digit of the GPS speed repeatedly (for example, 65 km/h appears as 5555). It cannot display 0065 until the digit commons are independently switched with suitable drivers.

### NEO-6M GPS

| GPS module pin | STM32F103C8T6 |
|---|---|
| TX | PA10 (USART1_RX) |
| GND | GND |
| VCC | Supply supported by your particular GPS breakout |
| RX | Not connected |

USART1 is configured for 9600 baud, 8 data bits, no parity, 1 stop bit, receive-only. USART1 global interrupt is enabled. PA9 is unused.

## Clock and debug

- HSE: 8 MHz crystal
- SYSCLK: 72 MHz (HSE × 9)
- APB1: 36 MHz
- APB2: 72 MHz
- SWD: PA13 / PA14

## Firmware behavior

1. Startup: six outer segments chase twice, followed by three all-segment flashes.
2. GPS search: a rotating outer-segment animation runs until the first valid RMC fix.
3. Valid GPS: parses `$GPRMC` and `$GNRMC`; RMC status must be `A`. Speed in knots is converted to km/h (× 1.852), rounded, and its ones digit is shown.
4. Speed update: briefly alternates the digit with all segments illuminated when the integer km/h value changes.
5. GPS lost: after a valid fix has previously been received, an invalid RMC status or a 3-second valid-data timeout makes the zero pattern blink.

UART reception is interrupt-driven one byte at a time. The callback only collects complete lines; NMEA parsing runs in the main loop.

## Build

Open `GPS_Speedometer.ioc` in STM32CubeIDE/CubeMX, verify the pinout above, generate code if needed, then build the project. Do not regenerate code from a stale .ioc copy.

The GitHub update has not been compiled on the user's local STM32CubeIDE toolchain; build and test on the target board before relying on it.
