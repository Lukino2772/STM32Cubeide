#ifndef PD_RENDER3D_H
#define PD_RENDER3D_H
/* Fixed-point wireframe 3D renderer. No heap, libm, framebuffer or MCU HAL.
 * Vertex coordinates Q8.8, sin/cos Q1.15. Optional STM32 CORDIC may supply
 * sin/cos via the application; portable lookup functions are provided.
 */
#include "pd_bus.h"
typedef struct {
    int32_t x, y, z;
} pd_r3d_vec_t;
typedef struct {
    int16_t sine, cosine;
} pd_r3d_angle_t;
typedef struct {
    uint16_t a, b;
} pd_r3d_edge_t;
typedef struct {
    uint16_t width, height;
    int16_t center_x, center_y;
    uint16_t focal_x, focal_y;
    int32_t depth_offset_q8, near_q8;
} pd_r3d_camera_t;
typedef pd_status_t (*pd_r3d_line_fn)(void*, int16_t, int16_t, int16_t, int16_t);
pd_r3d_angle_t pd_r3d_angle(uint16_t phase); /* phase: 0..65535 is full circle */
pd_status_t pd_r3d_rotate(pd_r3d_vec_t v, pd_r3d_angle_t rx, pd_r3d_angle_t ry, pd_r3d_angle_t rz,
                          pd_r3d_vec_t* out);
pd_status_t pd_r3d_project(const pd_r3d_camera_t*, pd_r3d_vec_t, int32_t* x, int32_t* y);
/* Invalid edges error; invisible vertices (z <= near) are skipped.
 * Lines are viewport-clipped before reaching draw() callback.
 */
pd_status_t pd_r3d_wire(const pd_r3d_camera_t*, const pd_r3d_vec_t*, size_t vertex_count,
                        const pd_r3d_edge_t*, size_t edge_count, pd_r3d_angle_t rx,
                        pd_r3d_angle_t ry, pd_r3d_angle_t rz, pd_r3d_line_fn, void* ctx,
                        size_t* drawn);
/* Optional caller-owned cache: transform/project each vertex once per frame.
 * Useful for dense meshes where vertices participate in many edges. */
typedef struct {
    int32_t x, y;
    bool visible;
} pd_r3d_projected_t;
pd_status_t pd_r3d_wire_cached(const pd_r3d_camera_t*, const pd_r3d_vec_t*, size_t,
                               const pd_r3d_edge_t*, size_t, pd_r3d_angle_t, pd_r3d_angle_t,
                               pd_r3d_angle_t, pd_r3d_projected_t* scratch, size_t scratch_capacity,
                               pd_r3d_line_fn, void*, size_t*);
#endif
