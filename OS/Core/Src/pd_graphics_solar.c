#include "pd_graphics_demo_internal.h"
#include "pd_render3d.h"

#define SOLAR_BG (0x0007U)
#define SOLAR_SUN (0xFCE0U)
#define SOLAR_ORBIT (0x2947U)
#define SOLAR_TILE_RADIUS (5)

static const uint8_t orbit_radius[PD_GFX_DEMO_PLANETS] =
    {14U,24U,34U,45U,56U,68U,80U,95U};
static const uint8_t eccentricity[PD_GFX_DEMO_PLANETS] =
    {21U,7U,2U,9U,5U,6U,5U,1U};
static const uint8_t planet_radius[PD_GFX_DEMO_PLANETS] =
    {2U,3U,3U,2U,5U,4U,3U,3U};
static const uint16_t planet_color[PD_GFX_DEMO_PLANETS] = {
    0x9CD2U,0xFDB9U,0x3BB8U,0xE3E8U,
    0xDB83U,0xF6ACU,0x66DDU,0x345EU
};
/* 2^32 / virtual demo orbital period (milliseconds). Fixed-point
 * acceleration compresses real planetary years for visual inspection.
 * Every planet still has an independent, smooth orbital rate. */
static const uint32_t degrees_per_ms[PD_GFX_DEMO_PLANETS] = {
    715827UL, 477218UL, 343597UL, 238609UL, 165191UL, 122713UL, 93368UL, 74051UL
};
static uint16_t smaller(uint16_t a,uint16_t b) {
    return a < b ? a : b;
}
static int32_t abs32(int32_t v) {
    return v < 0 ? -v : v;
}
static int32_t scaled_radius(const pd_gfx_demo_t *d,size_t index) {
    const uint16_t extent = smaller(d->screen.width,d->screen.height);
    return (int32_t)((uint32_t)orbit_radius[index] * extent / 240U);
}
static void solar_position(const pd_gfx_demo_t *d,size_t index,
                           int16_t *x,int16_t *y) {
    const pd_r3d_angle_t a = pd_r3d_angle(
        (uint16_t)(d->orbit_phase[index] >> 16U));
    const int32_t r = scaled_radius(d,index);
    const int32_t sx = r * (int32_t)a.cosine / 32767;
    const int32_t sy = r * (int32_t)a.sine / 32767;
    const int32_t shift = r * (int32_t)eccentricity[index] / 100;
    *x = (int16_t)((int32_t)d->screen.width / 2 + sx + sy / 7 - shift);
    *y = (int16_t)((int32_t)d->screen.height / 2 + sy * 11 / 20);
}
static uint16_t solar_background(const pd_gfx_demo_t *d,
                                 int32_t x,int32_t y) {
    const int32_t cx = (int32_t)d->screen.width / 2;
    const int32_t cy = (int32_t)d->screen.height / 2;
    const int32_t dx = x-cx;
    const int32_t dy = y-cy;
    /* A little deterministic starfield with no bitmap or RNG state. */
    uint32_t hash = (uint32_t)x * 2654435761U +
                    (uint32_t)y * 2246822519U + 0x8375ABCDU;
    hash ^= hash >> 16U;
    hash *= 2246822519UL;
    hash ^= hash >> 13U;
    uint16_t color = SOLAR_BG;
    if ((hash & 0x3FFU) < 3U)
        color = (hash & 0x400U) != 0U ? 0xAD55U : 0x528AU;
    /* This is the inverse of solar_position's skewed/flattened orbits,
     * evaluated in integer screen space. Tiles reproduce background
     * exactly: no full-frame repaint for moving planets. */
    const int32_t b = dy * 20 / 11;
    for (size_t i = 0U;i<PD_GFX_DEMO_PLANETS;++i) {
        const int32_t r = scaled_radius(d,i);
        if (r < 2) continue;
        const int32_t focus = r * (int32_t)eccentricity[i] / 100;
        const int32_t a = dx - b / 7 + focus;
        const int32_t distance = a*a + b*b;
        const int32_t r2 = r*r;
        if (abs32(distance-r2) <= r*2+1)
            color = SOLAR_ORBIT;
    }
    const int32_t sunr = (int32_t)smaller(d->screen.width,
                                         d->screen.height)/45;
    if (dx*dx+dy*dy <= sunr*sunr) {
        color = dx*dx+dy*dy <= (sunr-2)*(sunr-2) ?
                SOLAR_SUN : 0xFD60U;
    }
    return color;
}
static uint16_t tinted(uint16_t rgb, bool highlight) {
    if (!highlight) return rgb;
    /* Slightly brighter upper-left lit hemisphere. Saturating RGB565. */
    uint16_t r = (uint16_t)((rgb >> 11U)&31U);
    uint16_t g = (uint16_t)((rgb >> 5U)&63U);
    uint16_t b = (uint16_t)(rgb&31U);
    if (r < 29U) r = (uint16_t)(r+2U);
    if (g < 60U) g = (uint16_t)(g+3U);
    if (b < 29U) b = (uint16_t)(b+2U);
    return (uint16_t)((r<<11U)|(g<<5U)|b);
}
static uint16_t solar_pixel(const pd_gfx_demo_t *d,
                            int32_t x,int32_t y) {
    uint16_t result = solar_background(d,x,y);
    /* Composite all bodies in stable order so overlapping dirty
     * tiles cannot erase a still-visible neighboring planet. */
    for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i) {
        const int32_t dx=x-(int32_t)d->new_x[i];
        const int32_t dy=y-(int32_t)d->new_y[i];
        const int32_t r=(int32_t)planet_radius[i];
        if (dx*dx+dy*dy > r*r) continue;
        result = tinted(planet_color[i],dx+dy<0);
        if (i==5U && (dy == -r || dy == r) && dx >= -r-1 && dx <= r+1)
            result = 0xCEB2U; /* compact stylized Saturn band */
    }
    return result;
}
static pd_status_t solar_full_background(pd_gfx_demo_t *d) {
    /* No frame buffer: stream a single row in chunks of caller-selected
     * size; 121 RGB565 pixels (242 bytes) are sufficient. */
    for (uint16_t y=0U;y<d->screen.height;++y) {
        uint16_t x=0U;
        while (x < d->screen.width) {
            size_t count = (size_t)(d->screen.width - x);
            if (count > d->scratch_pixels)
                count = d->scratch_pixels;
            for (size_t i=0U;i<count;++i)
                d->scratch[i]=solar_pixel(d,(int32_t)x+(int32_t)i,y);
            const pd_status_t status = d->screen.blit(
                d->screen.ctx,x,y,(uint16_t)count,1U,
                d->scratch,count);
            if (status != PD_OK) return status;
            x = (uint16_t)((size_t)x+count);
        }
    }
    return PD_OK;
}
static pd_status_t solar_tile(pd_gfx_demo_t *d,int16_t cx,int16_t cy) {
    if (cx < -SOLAR_TILE_RADIUS || cy < -SOLAR_TILE_RADIUS ||
        cx >= (int32_t)d->screen.width + SOLAR_TILE_RADIUS ||
        cy >= (int32_t)d->screen.height + SOLAR_TILE_RADIUS)
        return PD_OK;
    int32_t left=(int32_t)cx-SOLAR_TILE_RADIUS;
    int32_t top=(int32_t)cy-SOLAR_TILE_RADIUS;
    int32_t right=(int32_t)cx+SOLAR_TILE_RADIUS;
    int32_t bottom=(int32_t)cy+SOLAR_TILE_RADIUS;
    if (left < 0) left=0;
    if (top < 0) top=0;
    if (right >= d->screen.width) right=(int32_t)d->screen.width-1;
    if (bottom >= d->screen.height) bottom=(int32_t)d->screen.height-1;
    const uint16_t w=(uint16_t)(right-left+1);
    const uint16_t h=(uint16_t)(bottom-top+1);
    const size_t count=(size_t)w*(size_t)h;
    /* 11x11 maximum, always fits the initialization requirement. */
    for (uint16_t row=0U;row<h;++row)
        for (uint16_t col=0U;col<w;++col)
            d->scratch[(size_t)row*w+col]=solar_pixel(
                d,left+(int32_t)col,top+(int32_t)row);
    return d->screen.blit(d->screen.ctx,(uint16_t)left,(uint16_t)top,
                          w,h,d->scratch,count);
}
pd_status_t pd_gfx_demo_solar_frame(pd_gfx_demo_t *d,uint32_t now_ms,
                                    bool fresh_scene) {
    if (!d->solar_ready || fresh_scene) {
        for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i)
            d->orbit_phase[i]=(uint32_t)(i*0x1B000000UL);
        d->last_solar_ms=now_ms;
        for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i)
            solar_position(d,i,&d->new_x[i],&d->new_y[i]);
        const pd_status_t status=solar_full_background(d);
        if (status != PD_OK) return status;
        for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i) {
            d->old_x[i]=d->new_x[i];
            d->old_y[i]=d->new_y[i];
        }
        d->solar_ready=true;
        return PD_OK;
    }
    uint32_t elapsed=(uint32_t)(now_ms-d->last_solar_ms);
    d->last_solar_ms=now_ms;
    /* Cap big redraw stalls: no minute-sized jumps after a paused UI. */
    if (elapsed>250U) elapsed=250U;
    if (elapsed==0U || d->solar_speed_q8==0U) return PD_OK;
    for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i) {
        const uint64_t delta=(uint64_t)elapsed *
                              (uint64_t)degrees_per_ms[i] *
                              (uint64_t)d->solar_speed_q8 / 256ULL;
        d->orbit_phase[i]+=(uint32_t)delta;
        solar_position(d,i,&d->new_x[i],&d->new_y[i]);
    }
    bool changed=false;
    for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i) {
        if (d->old_x[i] != d->new_x[i] ||
            d->old_y[i] != d->new_y[i]) {
            changed=true;
            break;
        }
    }
    if (!changed) return PD_OK;
    /* All new positions are computed before any pixel is sent.
     * Each tile composites every new body against regenerated sky,
     * fixing prior erase/overlap flicker without a framebuffer. */
    for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i) {
        if (d->old_x[i] == d->new_x[i] &&
            d->old_y[i] == d->new_y[i]) continue;
        pd_status_t s=solar_tile(d,d->old_x[i],d->old_y[i]);
        if (s != PD_OK) return s;
        s=solar_tile(d,d->new_x[i],d->new_y[i]);
        if (s != PD_OK) return s;
    }
    for (size_t i=0U;i<PD_GFX_DEMO_PLANETS;++i) {
        d->old_x[i]=d->new_x[i];
        d->old_y[i]=d->new_y[i];
    }
    return PD_OK;
}
