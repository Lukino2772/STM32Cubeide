#include "pd_graphics_benchmark.h"

static const uint16_t colors[8] = {
    0xF800U,0x07E0U,0x001FU,0xFFE0U,
    0xF81FU,0x07FFU,0xFFFFU,0x0000U
};
static const char *const stages[PD_GFX_BENCH_STAGE_COUNT] = {
    "FILL", "CHECKER", "BARS", "LINES", "RECTS", "PIXELS"
};
const char *pd_gfx_benchmark_stage_name(pd_gfx_bench_stage_t stage) {
    if ((unsigned)stage >= PD_GFX_BENCH_STAGE_COUNT)
        return "UNKNOWN";
    return stages[stage];
}
static pd_status_t fill(const pd_gfx_display_t *s,
                         pd_gfx_bench_result_t *m,
                         uint16_t x,uint16_t y,
                         uint16_t w,uint16_t h,uint16_t c) {
    const pd_status_t status=s->fill(s->ctx,x,y,w,h,c);
    if (status == PD_OK) {
        ++m->fill_calls;
        m->addressed_pixels+=(uint32_t)w*(uint32_t)h;
    }
    return status;
}
static pd_status_t line(const pd_gfx_display_t *s,
                         pd_gfx_bench_result_t *m,
                         int16_t x0,int16_t y0,
                         int16_t x1,int16_t y1,uint16_t c) {
    const pd_status_t status=s->line(s->ctx,x0,y0,x1,y1,c);
    if (status==PD_OK) ++m->line_calls;
    return status;
}
static size_t append_text(char *dst,size_t index,size_t cap,const char *src) {
    while (*src != '\0' && index+1U < cap)
        dst[index++]=*src++;
    dst[index]='\0';
    return index;
}
static size_t append_uint(char *dst,size_t index,size_t cap,uint32_t n) {
    char reversed[10] = {0};
    size_t length=0U;
    do {
        reversed[length++]=(char)('0'+(char)(n%10U));
        n/=10U;
    } while (n!=0U && length<sizeof(reversed));
    while (length!=0U && index+1U<cap)
        dst[index++]=reversed[--length];
    dst[index]='\0';
    return index;
}
static pd_status_t summary_line(const pd_gfx_display_t *s,
                                 uint16_t y,const char *label,uint32_t ms) {
    char text[32] = {0};
    size_t index=append_text(text,0U,sizeof(text),label);
    index=append_text(text,index,sizeof(text)," ");
    index=append_uint(text,index,sizeof(text),ms);
    (void)append_text(text,index,sizeof(text)," ms");
    return s->text(s->ctx,4U,y,text,0xFFFFU,0x0000U);
}
static pd_status_t stage(const pd_gfx_display_t *s,
                          pd_gfx_bench_result_t *m,
                          pd_gfx_bench_stage_t phase) {
    const uint16_t w=s->width,h=s->height;
    pd_status_t st=PD_OK;
    if (phase==PD_GFX_BENCH_FILLS) {
        /* Measure five sequential full-screen writes. */
        for (uint16_t i=0U;i<5U;++i) {
            st=fill(s,m,0U,0U,w,h,colors[i]);
            if (st!=PD_OK) return st;
        }
    } else if (phase==PD_GFX_BENCH_CHECKERS) {
        for (uint16_t y=0U;y<h;y=(uint16_t)(y+16U)) {
            const uint16_t tile_h=(uint16_t)(
                (uint32_t)h-(uint32_t)y<16U ? (uint32_t)h-(uint32_t)y : 16U);
            for (uint16_t x=0U;x<w;x=(uint16_t)(x+16U)) {
                const uint16_t tile_w=(uint16_t)(
                    (uint32_t)w-(uint32_t)x<16U ? (uint32_t)w-(uint32_t)x : 16U);
                const uint16_t color=(((x/16U)+(y/16U))&1U) ?
                                     0xFFFFU : 0x2104U;
                st=fill(s,m,x,y,tile_w,tile_h,color);
                if (st!=PD_OK) return st;
            }
        }
    } else if (phase==PD_GFX_BENCH_COLOR_BARS) {
        for (uint16_t i=0U;i<8U;++i) {
            const uint16_t x=(uint16_t)((uint32_t)i*w/8U);
            const uint16_t next=(uint16_t)((uint32_t)(i+1U)*w/8U);
            st=fill(s,m,x,0U,(uint16_t)(next-x),h,colors[i]);
            if (st!=PD_OK) return st;
        }
    } else if (phase==PD_GFX_BENCH_LINES) {
        st=fill(s,m,0U,0U,w,h,0x0000U);
        if (st!=PD_OK) return st;
        for (uint16_t x=0U;x<w;x=(uint16_t)(x+12U)) {
            st=line(s,m,(int16_t)x,0,(int16_t)x,(int16_t)(h-1U),0x07E0U);
            if (st!=PD_OK) return st;
        }
        for (uint16_t y=0U;y<h;y=(uint16_t)(y+12U)) {
            st=line(s,m,0,(int16_t)y,(int16_t)(w-1U),(int16_t)y,0x07FFU);
            if (st!=PD_OK) return st;
        }
        for (uint16_t i=0U;i<16U;++i) {
            const uint16_t y=(uint16_t)((uint32_t)i*(h-1U)/15U);
            st=line(s,m,0,(int16_t)y,(int16_t)(w-1U),
                    (int16_t)((h-1U)-y),0xF81FU);
            if (st!=PD_OK) return st;
        }
    } else if (phase==PD_GFX_BENCH_RECTANGLES) {
        st=fill(s,m,0U,0U,w,h,0x0000U);
        if (st!=PD_OK) return st;
        for (uint16_t inset=5U;inset<(uint16_t)(w/2U) &&
                                  inset<(uint16_t)(h/2U);
             inset=(uint16_t)(inset+8U)) {
            const int16_t x0=(int16_t)inset;
            const int16_t y0=(int16_t)inset;
            const int16_t x1=(int16_t)(w-inset-1U);
            const int16_t y1=(int16_t)(h-inset-1U);
            const uint16_t color=colors[(inset/8U)%7U];
            st=line(s,m,x0,y0,x1,y0,color);
            if (st!=PD_OK) return st;
            st=line(s,m,x1,y0,x1,y1,color);
            if (st!=PD_OK) return st;
            st=line(s,m,x1,y1,x0,y1,color);
            if (st!=PD_OK) return st;
            st=line(s,m,x0,y1,x0,y0,color);
            if (st!=PD_OK) return st;
        }
    } else if (phase==PD_GFX_BENCH_PIXELS) {
        st=fill(s,m,0U,0U,w,h,0x0000U);
        if (st!=PD_OK) return st;
        uint32_t state=0x93C7B2A5UL;
        for (uint16_t i=0U;i<512U;++i) {
            state^=state<<13U;
            state^=state>>17U;
            state^=state<<5U;
            const uint16_t x=(uint16_t)(state%w);
            const uint16_t y=(uint16_t)((state>>16U)%h);
            st=fill(s,m,x,y,1U,1U,colors[i%7U]);
            if (st!=PD_OK) return st;
        }
    } else {
        return PD_ERANGE;
    }
    return PD_OK;
}
pd_status_t pd_gfx_benchmark_run(const pd_gfx_display_t *screen,
                                 pd_gfx_bench_result_t *result) {
    if (screen==NULL || result==NULL || screen->fill==NULL ||
        screen->line==NULL || screen->text==NULL || screen->millis==NULL)
        return PD_EINVAL;
    /* The eight result lines need at least 160 px of vertical room. */
    if (screen->width<128U || screen->height<160U ||
        screen->width>PD_GFX_DEMO_MAX_DIMENSION ||
        screen->height>PD_GFX_DEMO_MAX_DIMENSION)
        return PD_ERANGE;
    pd_gfx_bench_result_t measured = {{0U},0U,0U,0U,0U};
    const uint32_t begin=screen->millis(screen->ctx);
    for (uint16_t i=0U;i<PD_GFX_BENCH_STAGE_COUNT;++i) {
        const uint32_t before=screen->millis(screen->ctx);
        const pd_status_t status=stage(screen,&measured,
                                     (pd_gfx_bench_stage_t)i);
        if (status!=PD_OK) return status;
        measured.stage_ms[i]=(uint32_t)(
            screen->millis(screen->ctx)-before);
    }
    measured.total_ms=(uint32_t)(screen->millis(screen->ctx)-begin);
    /* The finish/result screen is intentionally outside timed sections. */
    pd_status_t status=screen->fill(screen->ctx,0U,0U,
                                    screen->width,screen->height,0x0000U);
    if (status!=PD_OK) return status;
    status=screen->text(screen->ctx,4U,4U,
                        "DISPLAY BENCHMARK DONE",0x07FFU,0x0000U);
    if (status!=PD_OK) return status;
    status=summary_line(screen,22U,"TOTAL",measured.total_ms);
    if (status!=PD_OK) return status;
    for (uint16_t i=0U;i<PD_GFX_BENCH_STAGE_COUNT;++i) {
        status=summary_line(screen,(uint16_t)(43U+i*19U),
                 pd_gfx_benchmark_stage_name((pd_gfx_bench_stage_t)i),
                 measured.stage_ms[i]);
        if (status!=PD_OK) return status;
    }
    *result=measured;
    return PD_OK;
}
