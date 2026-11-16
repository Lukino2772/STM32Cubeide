#ifndef PD_BUS_H
#define PD_BUS_H
/* Hardware-independent C99 transport contract. No HAL, RTOS, malloc, or project headers.
 * Callbacks must complete synchronously and return PD_OK only on full transfer.
 * addr7 is a 7-BIT address: STM32 HAL adapters must shift by one at the HAL boundary.
 * Caller owns bus arbitration/locking; never interleave transactions on a shared bus.
 * The callbacks must enforce hardware timeouts; don't call blocking drivers from ISRs. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef enum {
    PD_OK = 0,
    PD_EINVAL = -1,
    PD_EIO = -2,
    PD_ETIMEOUT = -3,
    PD_ENODEV = -4,
    PD_EBUSY = -5,
    PD_ERANGE = -6,
    PD_ECRC = -7,
    PD_ENOTREADY = -8,
    PD_ENOTSUP = -9
} pd_status_t;
/* Diagnostic names for UART/USB logs. No heap or global error strings
 * requiring initialization; unknown status codes are handled safely. */
static inline const char *pd_status_name(pd_status_t status) {
    switch(status) {
        case PD_OK: return "PD_OK";
        case PD_EINVAL: return "PD_EINVAL";
        case PD_EIO: return "PD_EIO";
        case PD_ETIMEOUT: return "PD_ETIMEOUT";
        case PD_ENODEV: return "PD_ENODEV";
        case PD_EBUSY: return "PD_EBUSY";
        case PD_ERANGE: return "PD_ERANGE";
        case PD_ECRC: return "PD_ECRC";
        case PD_ENOTREADY: return "PD_ENOTREADY";
        case PD_ENOTSUP: return "PD_ENOTSUP";
        default: return "PD_EUNKNOWN";
    }
}
/* Optional application-owned bus lock. acquire() MUST have a bounded wait
 * and return PD_OK only after exclusive ownership. It must be nonrecursive
 * if callbacks can call back into the router. No RTOS objects are created. */
typedef struct {
    void *ctx;
    pd_status_t (*acquire)(void *);
    void (*release)(void *);
} pd_bus_lock_t;
typedef struct {
    void* ctx;
    pd_status_t (*write)(void*, uint8_t, const uint8_t*, size_t);
    pd_status_t (*read)(void*, uint8_t, uint8_t*, size_t);
    pd_status_t (*write_read)(void*, uint8_t, const uint8_t*, size_t, uint8_t*, size_t);
    pd_status_t (*probe)(void*, uint8_t); /* optional, except for EEPROM ack polling */
} pd_i2c_t;
typedef struct {
    void* ctx;
    /* Synchronous SPI full-duplex. NULL tx sends dummy 0xFF; NULL rx discards. */
    pd_status_t (*transfer)(void*, const uint8_t*, uint8_t*, size_t);
    /* selected=true asserts device CS, false deasserts it. Must be non-NULL. */
    void (*select)(void*, bool);
} pd_spi_t;
typedef struct {
    void* ctx;
    void (*write)(void*, bool); /* argument is physical logic HIGH=true */
    bool (*read)(void*);
} pd_pin_t;
typedef struct {
    void* ctx;
    void (*delay_ms)(void*, uint32_t);
    void (*delay_us)(void*, uint32_t);
    uint32_t (*millis)(void*);
} pd_time_t;
static inline pd_status_t pd_i2c_reg_read(const pd_i2c_t* b, uint8_t a, uint8_t reg, uint8_t* dst,
                                          size_t n) {
    if (!b || !b->write_read || (n && !dst) || (a > 0x7FU))
        return PD_EINVAL;
    if (!n)
        return PD_OK;
    return b->write_read(b->ctx, a, &reg, 1, dst, n);
}
static inline pd_status_t pd_i2c_reg_write8(const pd_i2c_t* b, uint8_t a, uint8_t reg, uint8_t v) {
    if (!b || !b->write || (a > 0x7FU))
        return PD_EINVAL;
    uint8_t tmp[2] = {reg, v};
    return b->write(b->ctx, a, tmp, 2);
}
static inline pd_status_t pd_i2c_reg_read16be(const pd_i2c_t* b, uint8_t a, uint8_t reg,
                                              uint16_t* out) {
    if (!out)
        return PD_EINVAL;
    uint8_t v[2];
    pd_status_t s = pd_i2c_reg_read(b, a, reg, v, 2);
    if (s == PD_OK)
        *out = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
    return s;
}
static inline pd_status_t pd_i2c_reg_write16be(const pd_i2c_t* b, uint8_t a, uint8_t reg,
                                               uint16_t value) {
    if (!b || !b->write || (a > 0x7FU))
        return PD_EINVAL;
    uint8_t p[3] = {reg, (uint8_t)(value >> 8), (uint8_t)value};
    return b->write(b->ctx, a, p, 3);
}
static inline pd_status_t pd_spi_write(const pd_spi_t* b, const uint8_t* p, size_t n) {
    if (!b || !b->transfer || (n && !p))
        return PD_EINVAL;
    return n ? b->transfer(b->ctx, p, NULL, n) : PD_OK;
}
#endif
