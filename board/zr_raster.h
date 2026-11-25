#ifndef MIDNRUN_RASTER_H
#define MIDNRUN_RASTER_H
#include "zr107.h"
#include "../video/texel_bus.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ZRRaster ZRRaster;
typedef struct { uint64_t swaps,words,triangles,fragments,rejected; } ZRRasterStats;
/* Attach once after zr_texel_init(). Width is the game's logical raster
   at the ZR107's native height of 384 (512 original, 683 widescreen). */
ZRRaster *zr_raster_create(ZRBoard *b,const ZRTexelBus *tex,unsigned width);
void zr_raster_destroy(ZRRaster *r);
void zr_draw_geometry(const ZRRaster *r,uint32_t *pixels,unsigned stride,unsigned width,unsigned height);
/* Interpolate 3D vertices at t=0.5, from the penultimate swap to the most
   recent swap. Compose the HUD afterwards. On cuts or static scenes this
   returns false without writing pixels. The completed swap remains intact. */
typedef struct { unsigned previous,current,matched,moving; } ZRMotionStats;
void zr_raster_enable_midpoints(ZRRaster *r,bool enable);
bool zr_draw_geometry_midpoint(ZRRaster *r,uint32_t *pixels,unsigned stride,unsigned width,unsigned height);
ZRMotionStats zr_raster_motion_stats(const ZRRaster *r);
ZRRasterStats zr_raster_stats(const ZRRaster *r);
#ifdef __cplusplus
}
#endif
#endif

