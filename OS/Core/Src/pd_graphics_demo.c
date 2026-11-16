#include "pd_graphics_demo_internal.h"
#include "pd_render3d.h"

#define GFX_BACKGROUND (0x0007U)
#define GFX_GRID (0x1082U)
#define GFX_FOREGROUND (0xFFFFU)
#define GFX_GOLD (0xFD20U)
#define GFX_CYAN (0x07FFU)
#define GFX_RED (0xF9C6U)

typedef struct {
    const pd_gfx_display_t *display;
    uint16_t color;
} gfx_wire_context_t;

static uint16_t smaller(uint16_t a, uint16_t b) {
    return a < b ? a : b;
}
static int32_t abs32(int32_t value) {
    return value < 0 ? -value : value;
}
static pd_status_t gfx_line(const pd_gfx_display_t *s,
                            int32_t x0, int32_t y0,
                            int32_t x1, int32_t y1, uint16_t color) {
    /* All drawing geometry is bounded to the 480x480 viewport.
     * Rejecting an offscreen edge avoids invalid device coordinates. */
    if (x0 < 0 || y0 < 0 || x1 < 0 || y1 < 0 ||
        x0 >= (int32_t)s->width || x1 >= (int32_t)s->width ||
        y0 >= (int32_t)s->height || y1 >= (int32_t)s->height)
        return PD_OK;
    return s->line(s->ctx, (int16_t)x0, (int16_t)y0,
                   (int16_t)x1, (int16_t)y1, color);
}
static pd_status_t wire_line(void *ctx, int16_t x0, int16_t y0,
                             int16_t x1, int16_t y1) {
    gfx_wire_context_t *wire = (gfx_wire_context_t *)ctx;
    return wire->display->line(wire->display->ctx, x0, y0,
                                x1, y1, wire->color);
}
static pd_r3d_camera_t camera(const pd_gfx_display_t *s) {
    const uint16_t extent = smaller(s->width, s->height);
    pd_r3d_camera_t c = {
        s->width, s->height,
        (int16_t)(s->width / 2U), (int16_t)(s->height / 2U),
        (uint16_t)((uint32_t)extent * 5U / 4U),
        (uint16_t)((uint32_t)extent * 5U / 4U),
        1200, 64
    };
    return c;
}
static pd_status_t edge3d(const pd_r3d_camera_t *cam,
                           gfx_wire_context_t *ctx,
                           pd_r3d_vec_t a, pd_r3d_vec_t b,
                           pd_r3d_angle_t rx, pd_r3d_angle_t ry,
                           pd_r3d_angle_t rz) {
    pd_r3d_vec_t va = {0}, vb = {0};
    pd_status_t s = pd_r3d_rotate(a, rx, ry, rz, &va);
    if (s != PD_OK) return s;
    s = pd_r3d_rotate(b, rx, ry, rz, &vb);
    if (s != PD_OK) return s;
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (pd_r3d_project(cam, va, &x0, &y0) != PD_OK ||
        pd_r3d_project(cam, vb, &x1, &y1) != PD_OK)
        return PD_OK;
    return gfx_line(ctx->display, x0, y0, x1, y1, ctx->color);
}
static pd_status_t background(const pd_gfx_display_t *s, bool grid) {
    pd_status_t status = s->fill(s->ctx, 0U, 0U, s->width,
                                  s->height, GFX_BACKGROUND);
    if (status != PD_OK || !grid) return status;
    for (uint16_t x = 24U; x < s->width; x = (uint16_t)(x + 24U)) {
        status = gfx_line(s, x, 0, x, (int32_t)s->height - 1, GFX_GRID);
        if (status != PD_OK) return status;
    }
    for (uint16_t y = 24U; y < s->height; y = (uint16_t)(y + 24U)) {
        status = gfx_line(s, 0, y, (int32_t)s->width - 1, y, GFX_GRID);
        if (status != PD_OK) return status;
    }
    return PD_OK;
}
static pd_status_t function_frame(const pd_gfx_display_t *s,
                                   pd_gfx_scene_t scene, uint32_t now) {
    const int32_t w = (int32_t)s->width;
    const int32_t h = (int32_t)s->height;
    const int32_t cx = w / 2;
    const int32_t cy = h / 2;
    const int32_t amp = h / 3;
    pd_status_t status = background(s, true);
    if (status != PD_OK) return status;
    status = gfx_line(s, 0, cy, w - 1, cy, 0x39E7U);
    if (status != PD_OK) return status;
    status = gfx_line(s, cx, 0, cx, h - 1, 0x39E7U);
    if (status != PD_OK) return status;
    int32_t previous_x = 0, previous_y = cy;
    const uint16_t sample_count = scene == PD_GFX_SCENE_LISSAJOUS ? 160U : s->width;
    for (uint16_t i = 0U; i <= sample_count; i = (uint16_t)(i + 2U)) {
        const int32_t t = (int32_t)(i > sample_count ? sample_count : i);
        int32_t x = t;
        int32_t y = cy;
        if (scene == PD_GFX_SCENE_SINE || scene == PD_GFX_SCENE_DAMPED_WAVE) {
            const uint16_t phase = (uint16_t)((uint32_t)t * 3U * 65536U /
                                               (uint32_t)w + now * 23U);
            const int32_t sine = pd_r3d_angle(phase).sine;
            int32_t magnitude = amp;
            if (scene == PD_GFX_SCENE_DAMPED_WAVE)
                magnitude = amp * (w - t) / w;
            y = cy - magnitude * sine / 32767;
        } else if (scene == PD_GFX_SCENE_PARABOLA) {
            const int32_t dx = t - cx;
            y = cy + amp / 2 - (amp * dx * dx) / (cx * cx);
        } else if (scene == PD_GFX_SCENE_EXPONENTIAL) {
            /* Five octaves approximated with linear interpolation. */
            const uint32_t u = (uint32_t)t * 5U * 256U / (uint32_t)w;
            const uint32_t octave = u >> 8U;
            const uint32_t fraction = u & 255U;
            const uint32_t scaled = (1U << octave) * (256U + fraction);
            y = h - 5 - (int32_t)(scaled * (uint32_t)(h - 12) / (32U * 256U));
        } else if (scene == PD_GFX_SCENE_LISSAJOUS) {
            const uint16_t phase = (uint16_t)((uint32_t)t * 65536U / 160U);
            x = cx + ((w / 3) *
                (int32_t)pd_r3d_angle((uint16_t)(phase * 3U + now * 9U)).sine) /
                32767;
            y = cy - ((h / 3) *
                (int32_t)pd_r3d_angle((uint16_t)(phase * 4U)).sine) / 32767;
        }
        if (x < 0) x = 0;
        if (x >= w) x = w - 1;
        if (y < 0) y = 0;
        if (y >= h) y = h - 1;
        if (i != 0U) {
            status = gfx_line(s, previous_x, previous_y, x, y,
                              scene == PD_GFX_SCENE_EXPONENTIAL ?
                              GFX_GOLD : GFX_CYAN);
            if (status != PD_OK) return status;
        }
        previous_x = x;
        previous_y = y;
        if (i == sample_count || (uint16_t)(i + 2U) > sample_count) break;
    }
    return PD_OK;
}
static pd_status_t cube_or_pyramid(const pd_gfx_display_t *s,
                                   bool pyramid, uint32_t now) {
    static const pd_r3d_vec_t cube[8] = {
        {-255,-255,-255},{255,-255,-255},{255,255,-255},{-255,255,-255},
        {-255,-255,255},{255,-255,255},{255,255,255},{-255,255,255}
    };
    static const pd_r3d_edge_t cube_edges[12] = {
        {0U,1U},{1U,2U},{2U,3U},{3U,0U},
        {4U,5U},{5U,6U},{6U,7U},{7U,4U},
        {0U,4U},{1U,5U},{2U,6U},{3U,7U}
    };
    static const pd_r3d_vec_t pyramid_vertices[5] = {
        {-270,-240,-270},{270,-240,-270},{270,-240,270},
        {-270,-240,270},{0,340,0}
    };
    static const pd_r3d_edge_t pyramid_edges[8] = {
        {0U,1U},{1U,2U},{2U,3U},{3U,0U},
        {0U,4U},{1U,4U},{2U,4U},{3U,4U}
    };
    const pd_r3d_camera_t c = camera(s);
    const pd_r3d_angle_t rx = pd_r3d_angle((uint16_t)(now * 7U + 4600U));
    const pd_r3d_angle_t ry = pd_r3d_angle((uint16_t)(now * 11U));
    const pd_r3d_angle_t rz = pd_r3d_angle((uint16_t)(now * 3U));
    gfx_wire_context_t ctx = {s, GFX_CYAN};
    size_t edges_drawn = 0U;
    if (pyramid)
        return pd_r3d_wire(&c, pyramid_vertices, 5U, pyramid_edges, 8U,
                           rx, ry, rz, wire_line, &ctx, &edges_drawn);
    return pd_r3d_wire(&c, cube, 8U, cube_edges, 12U, rx, ry, rz,
                       wire_line, &ctx, &edges_drawn);
}
static pd_r3d_vec_t sphere_point(int32_t radius,
                                  uint16_t latitude, uint16_t longitude) {
    const pd_r3d_angle_t a = pd_r3d_angle(latitude);
    const pd_r3d_angle_t b = pd_r3d_angle(longitude);
    const int32_t ring = radius * (int32_t)a.cosine / 32767;
    pd_r3d_vec_t v = {
        ring * (int32_t)b.cosine / 32767,
        ring * (int32_t)b.sine / 32767,
        radius * (int32_t)a.sine / 32767
    };
    return v;
}
static pd_r3d_vec_t torus_point(uint16_t u, uint16_t v) {
    const pd_r3d_angle_t major = pd_r3d_angle(u);
    const pd_r3d_angle_t minor = pd_r3d_angle(v);
    const int32_t ring = 245 + (90 * (int32_t)minor.cosine) / 32767;
    pd_r3d_vec_t p = {
        ring * (int32_t)major.cosine / 32767,
        ring * (int32_t)major.sine / 32767,
        (90 * (int32_t)minor.sine) / 32767
    };
    return p;
}
static pd_r3d_vec_t grid_point(int32_t ix, int32_t iy,
                                bool ripple, uint32_t now) {
    pd_r3d_vec_t p = {ix * 54, iy * 54, 0};
    if (ripple) {
        const uint16_t phase = (uint16_t)(
            (uint32_t)(abs32(ix) + abs32(iy)) * 5350U + now * 14U);
        p.z = (int32_t)pd_r3d_angle(phase).sine * 110 / 32767;
    } else {
        p.z = (ix * ix - iy * iy) * 6;
    }
    return p;
}
static pd_status_t procedural3d(const pd_gfx_display_t *s,
                                 pd_gfx_scene_t scene, uint32_t now) {
    const pd_r3d_camera_t c = camera(s);
    const pd_r3d_angle_t rx = pd_r3d_angle((uint16_t)(now * 5U + 8000U));
    const pd_r3d_angle_t ry = pd_r3d_angle((uint16_t)(now * 7U + 11000U));
    const pd_r3d_angle_t rz = pd_r3d_angle((uint16_t)(now * 2U));
    gfx_wire_context_t ctx = {s, GFX_CYAN};
    pd_status_t status = PD_OK;
    if (scene == PD_GFX_SCENE_HELIX) {
        for (int32_t i = 0; i < 112; ++i) {
            const pd_r3d_angle_t a = pd_r3d_angle((uint16_t)(
                (uint32_t)i * 5U * 65536U / 112U));
            const pd_r3d_angle_t b = pd_r3d_angle((uint16_t)(
                (uint32_t)(i + 1) * 5U * 65536U / 112U));
            pd_r3d_vec_t from = {
                (230 * (int32_t)a.cosine) / 32767,
                (230 * (int32_t)a.sine) / 32767,
                i * 6 - 336
            };
            pd_r3d_vec_t to = {
                (230 * (int32_t)b.cosine) / 32767,
                (230 * (int32_t)b.sine) / 32767,
                (i + 1) * 6 - 336
            };
            status = edge3d(&c, &ctx, from, to, rx, ry, rz);
            if (status != PD_OK) return status;
        }
    } else if (scene == PD_GFX_SCENE_SPHERE) {
        for (uint16_t lat = 0U; lat <= 6U; ++lat) {
            const uint16_t a = (uint16_t)((uint32_t)lat * 32768U / 6U -
                                           16384U);
            for (uint16_t lon = 0U; lon < 16U; ++lon) {
                const uint16_t b = (uint16_t)((uint32_t)lon * 65536U / 16U);
                const uint16_t next = (uint16_t)((uint32_t)(lon + 1U) *
                                                  65536U / 16U);
                status = edge3d(&c, &ctx, sphere_point(300, a, b),
                                sphere_point(300, a, next), rx, ry, rz);
                if (status != PD_OK) return status;
                if (lat != 6U) {
                    const uint16_t next_lat = (uint16_t)(
                        (uint32_t)(lat + 1U) * 32768U / 6U - 16384U);
                    status = edge3d(&c, &ctx, sphere_point(300, a, b),
                                    sphere_point(300, next_lat, b),
                                    rx, ry, rz);
                    if (status != PD_OK) return status;
                }
            }
        }
    } else if (scene == PD_GFX_SCENE_TORUS) {
        for (uint16_t u = 0U; u < 16U; ++u) {
            for (uint16_t v = 0U; v < 10U; ++v) {
                const uint16_t pa = (uint16_t)((uint32_t)u * 65536U / 16U);
                const uint16_t pb = (uint16_t)((uint32_t)v * 65536U / 10U);
                const uint16_t na = (uint16_t)((uint32_t)(u + 1U) *
                                                65536U / 16U);
                const uint16_t nb = (uint16_t)((uint32_t)(v + 1U) *
                                                65536U / 10U);
                status = edge3d(&c, &ctx, torus_point(pa,pb),
                                torus_point(na,pb),rx,ry,rz);
                if (status != PD_OK) return status;
                status = edge3d(&c, &ctx, torus_point(pa,pb),
                                torus_point(pa,nb),rx,ry,rz);
                if (status != PD_OK) return status;
            }
        }
    } else {
        const bool ripple = scene == PD_GFX_SCENE_RIPPLE;
        for (int32_t i = -5; i <= 5; ++i) {
            for (int32_t j = -5; j <= 5; ++j) {
                const pd_r3d_vec_t p = grid_point(i, j, ripple, now);
                if (i < 5) {
                    status = edge3d(&c,&ctx,p,grid_point(i+1,j,ripple,now),
                                    rx,ry,rz);
                    if (status != PD_OK) return status;
                }
                if (j < 5) {
                    status = edge3d(&c,&ctx,p,grid_point(i,j+1,ripple,now),
                                    rx,ry,rz);
                    if (status != PD_OK) return status;
                }
            }
        }
    }
    return PD_OK;
}
static pd_status_t circle(const pd_gfx_display_t *s, int32_t cx,
                           int32_t cy, int32_t radius, uint16_t color,
                           uint16_t offset) {
    int32_t prev_x = 0, prev_y = 0;
    for (uint16_t i = 0U; i <= 48U; ++i) {
        const pd_r3d_angle_t a = pd_r3d_angle((uint16_t)(
            (uint32_t)i * 65536U / 48U + offset));
        const int32_t x = cx + (radius * (int32_t)a.cosine) / 32767;
        const int32_t y = cy + (radius * (int32_t)a.sine) / 32767;
        if (i != 0U) {
            pd_status_t status = gfx_line(s,prev_x,prev_y,x,y,color);
            if (status != PD_OK) return status;
        }
        prev_x = x;
        prev_y = y;
    }
    return PD_OK;
}
static pd_status_t bob(const pd_gfx_display_t *s, int32_t x,
                        int32_t y, uint16_t color) {
    if (x < 4 || y < 4 || x + 4 >= s->width || y + 4 >= s->height)
        return PD_OK;
    return s->fill(s->ctx,(uint16_t)(x-4),(uint16_t)(y-4),
                   9U,9U,color);
}
static pd_status_t pendulum(const pd_gfx_display_t *s, uint32_t now,
                            bool double_pendulum) {
    const int32_t cx = (int32_t)s->width / 2;
    const int32_t py = (int32_t)s->height / 4;
    const int32_t length = (int32_t)smaller(s->width,s->height) /
                            (double_pendulum ? 4 : 3);
    const int32_t wave = pd_r3d_angle((uint16_t)(now * 13U)).sine;
    const uint16_t theta = (uint16_t)(wave * 7700 / 32767);
    const pd_r3d_angle_t angle = pd_r3d_angle(theta);
    const int32_t x1 = cx + length * (int32_t)angle.sine / 32767;
    const int32_t y1 = py + length * (int32_t)angle.cosine / 32767;
    pd_status_t status = gfx_line(s,cx - 25,py,cx + 25,py,0x9CD3U);
    if (status != PD_OK) return status;
    status = gfx_line(s,cx,py,x1,y1,GFX_FOREGROUND);
    if (status != PD_OK) return status;
    status = bob(s,x1,y1,GFX_GOLD);
    if (status != PD_OK || !double_pendulum) return status;
    const int32_t secondary = pd_r3d_angle((uint16_t)(now * 19U + 6000U)).sine;
    const uint16_t theta2 = (uint16_t)(secondary * 10000 / 32767);
    const pd_r3d_angle_t second = pd_r3d_angle(theta2);
    const int32_t x2 = x1 + length * (int32_t)second.sine / 32767;
    const int32_t y2 = y1 + length * (int32_t)second.cosine / 32767;
    status = gfx_line(s,x1,y1,x2,y2,GFX_CYAN);
    if (status != PD_OK) return status;
    return bob(s,x2,y2,GFX_RED);
}
static pd_status_t crank_slider(const pd_gfx_display_t *s, uint32_t now) {
    const int32_t radius = (int32_t)smaller(s->width,s->height) / 8;
    const int32_t cx = (int32_t)s->width / 3;
    const int32_t cy = (int32_t)s->height / 2;
    const pd_r3d_angle_t a = pd_r3d_angle((uint16_t)(now * 19U));
    const int32_t crank_x = cx + radius * (int32_t)a.cosine / 32767;
    const int32_t crank_y = cy + radius * (int32_t)a.sine / 32767;
    const int32_t slider_x = cx + radius * 3 +
                              radius * (int32_t)a.cosine / 32767;
    pd_status_t status = circle(s,cx,cy,radius,0x632CU,0U);
    if (status != PD_OK) return status;
    status = gfx_line(s,cx,cy,crank_x,crank_y,GFX_GOLD);
    if (status != PD_OK) return status;
    status = gfx_line(s,crank_x,crank_y,slider_x,cy,GFX_FOREGROUND);
    if (status != PD_OK) return status;
    status = gfx_line(s,cx+radius,cy+15,
                      (int32_t)s->width-8,cy+15,0x9CD3U);
    if (status != PD_OK) return status;
    status = bob(s,crank_x,crank_y,GFX_RED);
    if (status != PD_OK) return status;
    if (slider_x > 10 && slider_x + 10 < s->width && cy >= 10 &&
        cy + 10 < s->height) {
        status = s->fill(s->ctx,(uint16_t)(slider_x - 10),(uint16_t)(cy - 9),
                         21U,18U,GFX_CYAN);
    }
    return status;
}
static pd_status_t gear(const pd_gfx_display_t *s, int32_t cx,
                         int32_t cy, int32_t radius, uint16_t phase,
                         uint16_t color) {
    int32_t first_x = 0, first_y = 0;
    int32_t last_x = 0, last_y = 0;
    for (uint16_t i = 0U; i < 48U; ++i) {
        const pd_r3d_angle_t a = pd_r3d_angle((uint16_t)(
            (uint32_t)i * 65536U / 48U + phase));
        const int32_t r = (i % 4U) < 2U ? radius + 4 : radius;
        const int32_t x = cx + r * (int32_t)a.cosine / 32767;
        const int32_t y = cy + r * (int32_t)a.sine / 32767;
        if (i != 0U) {
            pd_status_t status = gfx_line(s,last_x,last_y,x,y,color);
            if (status != PD_OK) return status;
        } else {
            first_x = x; first_y = y;
        }
        last_x = x; last_y = y;
    }
    pd_status_t status = gfx_line(s,last_x,last_y,first_x,first_y,color);
    if (status != PD_OK) return status;
    for (uint16_t i=0U;i<4U;++i) {
        const pd_r3d_angle_t a = pd_r3d_angle((uint16_t)(
            phase + (uint16_t)(i * 16384U)));
        status = gfx_line(s,cx,cy,
                         cx+(radius-4)*(int32_t)a.cosine/32767,
                         cy+(radius-4)*(int32_t)a.sine/32767,color);
        if (status != PD_OK) return status;
    }
    return bob(s,cx,cy,0xBDF7U);
}
static pd_status_t gears(const pd_gfx_display_t *s, uint32_t now) {
    const int32_t radius = (int32_t)smaller(s->width,s->height) / 7;
    const int32_t cx = (int32_t)s->width / 2;
    const int32_t cy = (int32_t)s->height / 2;
    const uint16_t phase = (uint16_t)(now * 23U);
    pd_status_t status = gear(s,cx-radius-2,cy,radius,phase,GFX_GOLD);
    if (status != PD_OK) return status;
    return gear(s,cx+radius+2,cy,radius,
                (uint16_t)(32768U - phase),GFX_CYAN);
}
static pd_status_t gyroscope(const pd_gfx_display_t *s, uint32_t now) {
    /* Three precessing projected rings, with a dynamically tilted axis. */
    const pd_r3d_camera_t c = camera(s);
    gfx_wire_context_t ctx = {s,GFX_CYAN};
    for (uint16_t ring = 0U; ring < 3U; ++ring) {
        const pd_r3d_angle_t rx = pd_r3d_angle((uint16_t)(
            now * 4U + (uint32_t)ring * 16384U));
        const pd_r3d_angle_t ry = pd_r3d_angle((uint16_t)(now * 7U));
        const pd_r3d_angle_t rz = pd_r3d_angle((uint16_t)(now * 3U));
        pd_r3d_vec_t prev = {0};
        for (uint16_t i=0U;i<=48U;++i) {
            const pd_r3d_angle_t a = pd_r3d_angle((uint16_t)(
                (uint32_t)i * 65536U / 48U));
            const pd_r3d_vec_t p = {
                (280 * (int32_t)a.cosine) / 32767,
                (280 * (int32_t)a.sine) / 32767, 0
            };
            if (i != 0U) {
                pd_status_t status = edge3d(&c,&ctx,prev,p,rx,ry,rz);
                if (status != PD_OK) return status;
            }
            prev = p;
        }
        ctx.color = ring==0U ? GFX_GOLD :
                    ring==1U ? GFX_CYAN : GFX_RED;
    }
    return PD_OK;
}
static pd_status_t mechanics(const pd_gfx_display_t *s,
                              pd_gfx_scene_t scene,uint32_t now) {
    switch(scene) {
        case PD_GFX_SCENE_PENDULUM: return pendulum(s,now,false);
        case PD_GFX_SCENE_DOUBLE_PENDULUM: return pendulum(s,now,true);
        case PD_GFX_SCENE_CRANK_SLIDER: return crank_slider(s,now);
        case PD_GFX_SCENE_GEARS: return gears(s,now);
        case PD_GFX_SCENE_GYROSCOPE: return gyroscope(s,now);
        default: return PD_ERANGE;
    }
}
static const char *const scene_names[PD_GFX_SCENE_COUNT] = {
    "SINE WAVE", "DAMPED WAVE", "PARABOLA", "EXPONENTIAL",
    "LISSAJOUS", "3D CUBE", "3D PYRAMID", "3D SPHERE",
    "3D TORUS", "3D HELIX", "3D SADDLE", "3D RIPPLE",
    "SOLAR SYSTEM", "PENDULUM", "DOUBLE PENDULUM",
    "CRANK SLIDER", "GEAR TRAIN", "GYROSCOPE"
};
const char *pd_gfx_demo_scene_name(pd_gfx_scene_t scene) {
    if ((unsigned)scene >= (unsigned)PD_GFX_SCENE_COUNT)
        return "UNKNOWN";
    return scene_names[scene];
}
pd_status_t pd_gfx_demo_init(pd_gfx_demo_t *d,
                              const pd_gfx_display_t *screen,
                              uint16_t *scratch,size_t capacity) {
    if (d == NULL || screen == NULL || screen->fill == NULL ||
        screen->line == NULL || screen->blit == NULL || scratch == NULL)
        return PD_EINVAL;
    if (screen->width < 64U || screen->height < 64U ||
        screen->width > PD_GFX_DEMO_MAX_DIMENSION ||
        screen->height > PD_GFX_DEMO_MAX_DIMENSION ||
        capacity < PD_GFX_DEMO_MIN_SCRATCH_PIXELS)
        return PD_ERANGE;
    d->screen = *screen;
    d->scratch = scratch;
    d->scratch_pixels = capacity;
    d->last_solar_ms = 0U;
    d->solar_speed_q8 = 256U;
    d->previous_scene = PD_GFX_SCENE_COUNT;
    d->solar_ready = false;
    for (size_t i = 0U; i < PD_GFX_DEMO_PLANETS; ++i) {
        d->orbit_phase[i] = 0U;
        d->old_x[i] = -1; d->old_y[i] = -1;
        d->new_x[i] = -1; d->new_y[i] = -1;
    }
    d->initialized = true;
    return PD_OK;
}
void pd_gfx_demo_invalidate(pd_gfx_demo_t *d) {
    if (d != NULL) {
        d->previous_scene = PD_GFX_SCENE_COUNT;
        d->solar_ready = false;
    }
}
pd_status_t pd_gfx_demo_set_solar_speed(pd_gfx_demo_t *d,
                                        uint16_t speed_q8) {
    if (d == NULL || !d->initialized) return PD_ENOTREADY;
    if (speed_q8 > 4096U) return PD_ERANGE;
    d->solar_speed_q8 = speed_q8;
    return PD_OK;
}
pd_status_t pd_gfx_demo_render(pd_gfx_demo_t *d,
                                pd_gfx_scene_t scene,uint32_t now_ms) {
    if (d == NULL || !d->initialized) return PD_ENOTREADY;
    if ((unsigned)scene >= (unsigned)PD_GFX_SCENE_COUNT)
        return PD_ERANGE;
    pd_status_t status;
    if (scene == PD_GFX_SCENE_SOLAR_SYSTEM) {
        status = pd_gfx_demo_solar_frame(
            d,now_ms,d->previous_scene != PD_GFX_SCENE_SOLAR_SYSTEM);
    } else {
        /* A later return to solar must reconstruct its full sky before
         * continuing incremental planet tiles. */
        d->solar_ready = false;
        const pd_gfx_display_t *s = &d->screen;
        if (scene <= PD_GFX_SCENE_LISSAJOUS) {
            status = function_frame(s,scene,now_ms);
        } else {
            status = background(s,false);
            if (status == PD_OK) {
                if (scene <= PD_GFX_SCENE_PYRAMID)
                    status = cube_or_pyramid(s,
                        scene == PD_GFX_SCENE_PYRAMID,now_ms);
                else if (scene <= PD_GFX_SCENE_RIPPLE)
                    status = procedural3d(s,scene,now_ms);
                else
                    status = mechanics(s,scene,now_ms);
            }
        }
    }
    if (status == PD_OK && d->screen.text != NULL &&
        (scene != PD_GFX_SCENE_SOLAR_SYSTEM ||
         d->previous_scene != PD_GFX_SCENE_SOLAR_SYSTEM)) {
        /* The solar sky stays intact between frames; avoid rewriting its
         * title when only a small planet tile needs updating. */
        status = d->screen.text(d->screen.ctx,4U,4U,
                 pd_gfx_demo_scene_name(scene),GFX_FOREGROUND,GFX_BACKGROUND);
    }
    if (status != PD_OK) {
        pd_gfx_demo_invalidate(d);
        return status;
    }
    d->previous_scene = scene;
    return PD_OK;
}
