#include "pd_render3d.h"
#include <limits.h>
/* 65-entry quarter-wave table: 130 bytes of flash at int16_t. */
static const int16_t pd_sin_q15[65] = {
    0,     804,   1608,  2410,  3212,  4011,  4808,  5602,  6393,  7179,  7962,  8739,  9512,
    10278, 11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868,
    19519, 20159, 20787, 21403, 22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319,
    26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571, 30852, 31113,
    31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767};
static int16_t sin_phase(uint16_t p) {
    uint16_t q = (uint16_t)(p >> 14);
    uint16_t t = (uint16_t)(p & 16383U);
    if (q & 1U)
        t = (uint16_t)(16384U - t);
    unsigned i = (unsigned)(t >> 8);
    int32_t v = pd_sin_q15[i];
    if (i < 64U) {
        int32_t delta = (int32_t)pd_sin_q15[i + 1U] - v;
        v += (delta * (int32_t)(t & 255U)) / 256;
    }
    if (q >= 2U)
        v = -v;
    return (int16_t)v;
}
pd_r3d_angle_t pd_r3d_angle(uint16_t p) {
    pd_r3d_angle_t a = {sin_phase(p), sin_phase((uint16_t)(p + 16384U))};
    return a;
}
static int32_t rot(int32_t a, int32_t b, int32_t c, int32_t s) {
    return (int32_t)(((int64_t)a * c - (int64_t)b * s) / 32767);
}
static int32_t rot2(int32_t a, int32_t b, int32_t c, int32_t s) {
    return (int32_t)(((int64_t)a * s + (int64_t)b * c) / 32767);
}
pd_status_t pd_r3d_rotate(pd_r3d_vec_t v, pd_r3d_angle_t rx, pd_r3d_angle_t ry, pd_r3d_angle_t rz,
                          pd_r3d_vec_t* out) {
    if (!out)
        return PD_EINVAL;
    if (v.x < -(1 << 20) || v.x > (1 << 20) || v.y < -(1 << 20) || v.y > (1 << 20) ||
        v.z < -(1 << 20) || v.z > (1 << 20))
        return PD_ERANGE;
    /* Axis order: X then Y then Z. temp is bounded ~ 3x input range. */
    int32_t y = rot(v.y, v.z, rx.cosine, rx.sine);
    int32_t z = rot2(v.y, v.z, rx.cosine, rx.sine);
    int32_t x = rot(v.x, z, ry.cosine, -(int32_t)ry.sine);
    z = rot2(v.x, z, ry.cosine, -(int32_t)ry.sine);
    out->x = rot(x, y, rz.cosine, rz.sine);
    out->y = rot2(x, y, rz.cosine, rz.sine);
    out->z = z;
    return PD_OK;
}
static int32_t limit_screen(int64_t p) {
    if (p > 1000000LL)
        return 1000000;
    if (p < -1000000LL)
        return -1000000;
    return (int32_t)p;
}
pd_status_t pd_r3d_project(const pd_r3d_camera_t* c, pd_r3d_vec_t v, int32_t* x, int32_t* y) {
    if (!c || !x || !y || !c->width || !c->height || c->width > 4096U || c->height > 4096U ||
        c->near_q8 <= 0 || !c->focal_x || !c->focal_y)
        return PD_EINVAL;
    int64_t depth = (int64_t)v.z + c->depth_offset_q8;
    if (depth <= c->near_q8)
        return PD_ERANGE;
    *x = limit_screen((int64_t)c->center_x + ((int64_t)c->focal_x * v.x) / depth);
    *y = limit_screen((int64_t)c->center_y - ((int64_t)c->focal_y * v.y) / depth);
    return PD_OK;
}
static unsigned outcode(int32_t x, int32_t y, const pd_r3d_camera_t* c) {
    unsigned r = 0;
    if (x < 0)
        r |= 1U;
    else if (x >= (int32_t)c->width)
        r |= 2U;
    if (y < 0)
        r |= 4U;
    else if (y >= (int32_t)c->height)
        r |= 8U;
    return r;
}
static bool clip_line(const pd_r3d_camera_t* c, int32_t* x0, int32_t* y0, int32_t* x1,
                      int32_t* y1) {
    for (unsigned i = 0; i < 16; i++) {
        unsigned a = outcode(*x0, *y0, c), b = outcode(*x1, *y1, c);
        if (!(a | b))
            return true;
        if (a & b)
            return false;
        unsigned q = a ? a : b;
        int32_t x = 0, y = 0;
        int64_t dx = (int64_t)*x1 - *x0, dy = (int64_t)*y1 - *y0;
        if (q & 4U) {
            if (!dy)
                return false;
            y = 0;
            x = (int32_t)((int64_t)*x0 + dx * (0 - (int64_t)*y0) / dy);
        } else if (q & 8U) {
            if (!dy)
                return false;
            y = (int32_t)c->height - 1;
            x = (int32_t)((int64_t)*x0 + dx * (y - (int64_t)*y0) / dy);
        } else if (q & 2U) {
            if (!dx)
                return false;
            x = (int32_t)c->width - 1;
            y = (int32_t)((int64_t)*y0 + dy * (x - (int64_t)*x0) / dx);
        } else {
            if (!dx)
                return false;
            x = 0;
            y = (int32_t)((int64_t)*y0 + dy * (0 - (int64_t)*x0) / dx);
        }
        if (a) {
            *x0 = x;
            *y0 = y;
        } else {
            *x1 = x;
            *y1 = y;
        }
    }
    return false; /* finite iteration bound on malformed input */
}
pd_status_t pd_r3d_wire(const pd_r3d_camera_t* c, const pd_r3d_vec_t* v, size_t nv,
                        const pd_r3d_edge_t* edges, size_t ne, pd_r3d_angle_t rx, pd_r3d_angle_t ry,
                        pd_r3d_angle_t rz, pd_r3d_line_fn draw, void* ctx, size_t* drawn) {
    if (!c || !v || !edges || !draw || !drawn || !nv || nv > 4096U || ne > 16384U)
        return PD_EINVAL;
    *drawn = 0;
    for (size_t i = 0; i < ne; i++) {
        if (edges[i].a >= nv || edges[i].b >= nv)
            return PD_ERANGE;
        pd_r3d_vec_t a, b;
        pd_status_t s = pd_r3d_rotate(v[edges[i].a], rx, ry, rz, &a);
        if (s != PD_OK)
            return s;
        s = pd_r3d_rotate(v[edges[i].b], rx, ry, rz, &b);
        if (s != PD_OK)
            return s;
        int32_t x0, y0, x1, y1;
        if (pd_r3d_project(c, a, &x0, &y0) != PD_OK || pd_r3d_project(c, b, &x1, &y1) != PD_OK)
            continue;
        if (!clip_line(c, &x0, &y0, &x1, &y1))
            continue;
        s = draw(ctx, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1);
        if (s != PD_OK)
            return s;
        (*drawn)++;
    }
    return PD_OK;
}

pd_status_t pd_r3d_wire_cached(const pd_r3d_camera_t* c, const pd_r3d_vec_t* v, size_t nv,
                               const pd_r3d_edge_t* edges, size_t ne, pd_r3d_angle_t rx,
                               pd_r3d_angle_t ry, pd_r3d_angle_t rz, pd_r3d_projected_t* cache,
                               size_t capacity, pd_r3d_line_fn draw, void* ctx, size_t* drawn) {
    if (!c || !v || !edges || !cache || !draw || !drawn || !nv || nv > 4096U || ne > 16384U ||
        capacity < nv)
        return PD_EINVAL;
    *drawn = 0;
    for (size_t i = 0; i < nv; i++) {
        pd_r3d_vec_t p;
        pd_status_t s = pd_r3d_rotate(v[i], rx, ry, rz, &p);
        if (s != PD_OK)
            return s;
        cache[i].visible = (pd_r3d_project(c, p, &cache[i].x, &cache[i].y) == PD_OK);
    }
    for (size_t i = 0; i < ne; i++) {
        if (edges[i].a >= nv || edges[i].b >= nv)
            return PD_ERANGE;
        pd_r3d_projected_t a = cache[edges[i].a], b = cache[edges[i].b];
        if (!a.visible || !b.visible)
            continue;
        if (!clip_line(c, &a.x, &a.y, &b.x, &b.y))
            continue;
        pd_status_t s = draw(ctx, (int16_t)a.x, (int16_t)a.y, (int16_t)b.x, (int16_t)b.y);
        if (s != PD_OK)
            return s;
        ++*drawn;
    }
    return PD_OK;
}
