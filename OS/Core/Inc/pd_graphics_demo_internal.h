#ifndef PD_GRAPHICS_DEMO_INTERNAL_H
#define PD_GRAPHICS_DEMO_INTERNAL_H
#include "pd_graphics_demo.h"
/* Internal linkage between the optional demo scene modules. */
pd_status_t pd_gfx_demo_solar_frame(pd_gfx_demo_t *d, uint32_t now_ms,
                                    bool fresh_scene);
#endif
