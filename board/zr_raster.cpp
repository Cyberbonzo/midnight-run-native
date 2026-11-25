/* Software rasterization for the ZR107 FIFO. Geometry and texture coordinates
   follow the arcade register interface. Target dimensions use square pixels. */
#include "zr_raster.h"
#include "../video/zr107_video.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <future>
#include <unordered_map>
#include <vector>

namespace {
constexpr unsigned rows=384;
struct Vec {
    float x{},y{},z{},u{},v{},rw{1},diff{};
    std::array<float,4> rgba{};
};
static float as_float(uint32_t w) { float f; std::memcpy(&f,&w,4);return f; }
static unsigned sat(float x) { return unsigned(std::clamp(x,0.0f,255.0f)); }
static std::array<float,4> rgb(uint32_t c) {
    return {float((c>>16)&255),float((c>>8)&255),float(c&255),float(c>>24)};
}
static std::array<float,4> guest_color(uint32_t c) {
    return {float(c&255),float((c>>8)&255),float((c>>16)&255),float(c>>24)};
}
static uint32_t pack(const std::array<float,4>& c) {
    return (sat(c[3])<<24)|(sat(c[0])<<16)|(sat(c[1])<<8)|sat(c[2]);
}
static unsigned morton_lane(unsigned p, unsigned parity) {
    unsigned v=0;
    for(unsigned n=0;n<6;n++) v|=((p>>(n*2+parity))&1)<<n;
    return v;
}
static int wrap_texel(int x,unsigned span,bool mirror) {
    // Texture headers encode eight widths in multiples of eight. The device
    // only wires the bottom seven bits of each coordinate to the wrap logic.
    static constexpr auto wrapped=[] {
        std::array<std::array<std::array<uint8_t,128>,8>,2> table{};
        for(unsigned size=0;size<8;size++) {
            const unsigned count=(size+1)*8;
            for(unsigned i=0;i<128;i++) {
                const unsigned offset=i%count;
                table[0][size][i]=uint8_t(offset);
                table[1][size][i]=uint8_t(((i/count)&1)?count-1-offset:offset);
            }
        }
        return table;
    }();
    return wrapped[mirror][(span>>3)-1][unsigned(x)&127];
}
struct Surface {
    uint32_t cmd{};
    unsigned sx{},sy{},tw{},th{},page{},bank{};
    std::array<float,4> flat{};
};
}

#include "zr_motion.inc"

struct ZRRaster {
    ZRBoard *board{};
    const ZRTexelBus *tex{};
    unsigned width{},draw{},head{};
    std::vector<uint8_t> texels;
    std::array<uint32_t,65536> tex_color;
    std::vector<uint32_t> image[2], packet;
    std::vector<float> depth;
    Vec history[4]{};
    uint32_t regs[0x1a]{};
    ZRRasterStats stats{};
    bool dropped{};
    bool capture_motion{},middle_ready{},middle_valid{};
    std::vector<Primitive> building,previous,current;
    std::vector<float> middle_depth;
    std::vector<uint32_t> middle_image;
    ZRMotionStats motion{};

    uint32_t sample(const Surface &s,int x,int y) const {
        bool mirror=(s.cmd&16)!=0;
        unsigned tx=(s.sx+wrap_texel(x,s.tw,mirror))&511;
        unsigned ty=(s.sy+wrap_texel(y,s.th,mirror))&511;
        unsigned pen=texels[(s.page<<18)|(ty<<9)|tx];
        return pen ? tex_color[tex->palette[(s.bank<<8)|pen]] : 0;
    }
    std::array<float,4> texture(const Surface& s,int u,int v,bool &opaque,unsigned texture_select) const {
        if (!(texture_select&0x100000)) {
            auto c=rgb(sample(s,u>>4,v>>4));opaque=c[3]>0;return c;
        }
        u-=7;v-=7;
        int x=u>>4,y=v>>4;float f=(u&15)/16.f,g=(v&15)/16.f;
        auto a=rgb(sample(s,x,y)), b=rgb(sample(s,x+1,y));
        auto c=rgb(sample(s,x,y+1)), d=rgb(sample(s,x+1,y+1));
        opaque=a[3]>0;
        for(unsigned i=0;i<4;i++) a[i]=((1-f)*a[i]+f*b[i])*(1-g)+((1-f)*c[i]+f*d[i])*g;
        return a;
    }
    void triangle(const Vec &a,const Vec &b,const Vec &c,const Surface &s) {
        if(capture_motion && building.size()<32768) {
            Primitive p{{a,b,c},s,{},tex->select};
            std::copy_n(regs,0x1a,p.regs.begin());
            building.push_back(p);
        }
        rasterize(a,b,c,s,regs,tex->select,image[draw],depth,stats);
    }
    void rasterize(const Vec &a,const Vec &b,const Vec &c,const Surface &s,
                   const uint32_t *state,unsigned texture_select,std::vector<uint32_t>&frame,
                   std::vector<float>&zbuffer,ZRRasterStats&counts,int row_begin=0,int row_end=rows) {
        float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        if(!std::isfinite(area) || std::fabs(area)<1e-6f) return;
        const float ox=width*.5f;
        int minx=std::max(0,int(std::ceil(std::min({a.x,b.x,c.x})-.5f)));
        int maxx=std::min(int(width)-1,int(std::floor(std::max({a.x,b.x,c.x})-.5f)));
        int miny=std::max(row_begin,int(std::ceil(std::min({a.y,b.y,c.y})-.5f)));
        int maxy=std::min(row_end-1,int(std::floor(std::max({a.y,b.y,c.y})-.5f)));
        minx=std::max(minx,int(ox)+int16_t(state[3])+int16_t(state[7]));
        maxx=std::min(maxx,int(ox)+int16_t(state[4])+int16_t(state[7])+1);
        miny=std::max(miny,200+int16_t(state[6])-int16_t(state[8]));
        maxy=std::min(maxy,201+int16_t(state[5])-int16_t(state[8]));
        if(minx>maxx || miny>maxy) return;
        counts.triangles++;
        const bool textured=(s.cmd&24)!=0, vertex=(s.cmd&256)!=0;
        const bool fog=state[0x17]!=0xffff && !(s.cmd&1);
        float fogstart=as_float((0x90000u|(state[0x17]&65535))<<11);
        float fogend=as_float((0x90000u|(state[0x18]&65535))<<11);
        const float gx=(b.y-c.y)/area, gy=(c.x-b.x)/area;
        const float hx=(c.y-a.y)/area, hy=(a.x-c.x)/area;
        for(int y=miny;y<=maxy;y++) {
            float w0=gx*(minx+.5f-c.x)+gy*(y+.5f-c.y);
            float w1=hx*(minx+.5f-c.x)+hy*(y+.5f-c.y);
            for(int x=minx;x<=maxx;x++,w0+=gx,w1+=hx) {
                float w2=1-w0-w1;
                if(w0 < -1e-5f || w1 < -1e-5f || w2 < -1e-5f) continue;
                size_t index=size_t(y)*width+x;
                float z=a.z*w0+b.z*w1+c.z*w2;
                if(!std::isfinite(z) || ((s.cmd&4) && z>zbuffer[index])) continue;
                auto color=s.flat;
                if(vertex) { for(unsigned j=0;j<4;j++) color[j]=a.rgba[j]*w0+b.rgba[j]*w1+c.rgba[j]*w2; }
                float alpha=color[3];
                if(vertex && alpha<1) continue;
                if(textured) {
                    float rw=a.rw*w0+b.rw*w1+c.rw*w2;
                    if(!std::isfinite(rw) || rw<=0) continue;
                    float tu=(a.u*w0+b.u*w1+c.u*w2)/rw, tv=(a.v*w0+b.v*w1+c.v*w2)/rw;
                    if(!std::isfinite(tu) || !std::isfinite(tv) || std::fabs(tu)>1e9f || std::fabs(tv)>1e9f) continue;
                    bool opaque=false;
                    auto tex_color=texture(s,int(tu),int(tv),opaque,texture_select);
                    if(!opaque && !vertex) continue;
                    if(opaque) color=tex_color;
                }
                float diffuse=std::clamp(a.diff*w0+b.diff*w1+c.diff*w2,0.f,255.f);
                auto back=rgb(frame[index]);
                for(unsigned j=0;j<3;j++) {
                    float light=std::min(255.f,float(state[0xd+j]&255)+float(state[0xa+j]&255)*diffuse/255.f);
                    color[j]=color[j]*light/255.f;
                    if(fog && fogend!=fogstart) {
                        float near=std::clamp((fogend-z)/(fogend-fogstart),0.f,1.f);
                        color[j]=near*color[j]+(1-near)*float(state[0x10+j]&255);
                    }
                    float blend=1;
                    if((vertex || !textured) && !(s.cmd&2)) blend=std::clamp(alpha/255.f,0.f,1.f);
                    else if(textured && !vertex && (texture_select&0x100000)) blend=color[3]/255.f;
                    color[j]=color[j]*blend+back[j]*(1-blend);
                }
                frame[index]=pack(color);
                zbuffer[index]=z;counts.fragments++;
            }
        }
    }
    void flush() {
        size_t pos=0;
        bool failed=false;
        auto get=[&]() -> uint32_t { if(pos>=packet.size()) { failed=true;return 0; } return packet[pos++]; };
        auto is_cmd=[](uint32_t v) {return (v&0xffff0000)==0x80000000;};
        while(pos<packet.size() && !failed) {
            Surface surf{};surf.cmd=get();
            if(!is_cmd(surf.cmd) || surf.cmd==0x80000000 || surf.cmd==0x80000018) continue;
            bool texd=(surf.cmd&24)!=0,vcolor=(surf.cmd&256)!=0,vdepth=!(surf.cmd&1)||vcolor;
            if(texd) {
                uint32_t header=get();
                surf.sx=morton_lane(header,1)*8;surf.sy=morton_lane(header,0)*8;
                surf.tw=(((header>>23)&7)+1)*8;surf.th=(((header>>20)&7)+1)*8;
                surf.page=(header>>12)&31;surf.bank=(header>>28)&7;
            }
            while(pos<packet.size() && !failed && !is_cmd(packet[pos])) {
                unsigned added=0;bool last=false,quad=false;
                float poly_z=0,poly_diff=0;
                auto polydepth=[&](){uint32_t w=get();poly_z=as_float((w&0x07ffff00)|0x48000000);poly_diff=float(w&255);};
                do {
                    uint32_t word=get(); Vec v{};
                    auto xy=midnrun::Viewport{width,rows}.polygon_xy(word);
                    v.x=xy.x;v.y=xy.y;quad=(word&0x4000)!=0;last=(word&0x8000)!=0;
                    if(vdepth) {
                        uint32_t zw=get();v.z=as_float(zw&0xffffff00);v.diff=float(zw&255);
                        v.rw=v.z!=0?1.f/v.z:0;
                    }
                    if(last && texd) { if(!vdepth) polydepth(); if(!vcolor) surf.flat=guest_color(get()); }
                    if(vcolor) v.rgba=guest_color(get());
                    if(texd) { uint32_t uv=get();v.u=int16_t(uv>>16)*v.rw;v.v=int16_t(uv)*v.rw; }
                    history[head]=v;head=(head+1)&3;added++;
                } while(added<4 && !last && !failed);
                if(!texd) { if(!vdepth) polydepth(); if(!vcolor) surf.flat=guest_color(get()); }
                if(failed) break;
                if(!last) { while(pos<packet.size() && !is_cmd(packet[pos])) pos++; break; }
                if(!vdepth) for(Vec &v:history) {v.z=poly_z;v.diff=poly_diff;}
                auto &v0=history[head],&v1=history[(head+1)&3],&v2=history[(head+2)&3],&v3=history[(head+3)&3];
                if(!(surf.cmd&32) && added<3) { v0.diff=v1.diff=v2.diff=v3.diff; }
                if(quad) {triangle(v0,v1,v2,surf);triangle(v2,v3,v0,surf);}
                else triangle(v1,v2,v3,surf);
            }
        }
        if(failed) stats.rejected++;
        packet.clear();
    }
};

static void push(void *ptr,uint32_t word) {
    ZRRaster &r=*static_cast<ZRRaster*>(ptr);
    r.stats.words++;
    if(word==0x80000000) {
        if(!r.dropped) r.flush();
        r.dropped=false;
    }
    if(r.dropped) return;
    if(r.packet.size()>=65536) { r.packet.clear();r.dropped=true;r.stats.rejected++;return; }
    r.packet.push_back(word);
}
static void swap(void *ptr) {
    ZRRaster &r=*static_cast<ZRRaster*>(ptr);
    if(!r.dropped) r.flush();
    r.dropped=false;
    if(r.capture_motion) {
        r.previous.swap(r.current);r.current.swap(r.building);r.building.clear();
        r.middle_ready=false;r.middle_valid=false;
    }
    r.stats.swaps++;r.draw^=1;
    std::fill(r.image[r.draw].begin(),r.image[r.draw].end(),0);
    std::fill(r.depth.begin(),r.depth.end(),1e10f);
}
static void pixel_reg(void *ptr,unsigned reg,uint32_t value) {
    if(reg>=0x100 && reg<=0x119) static_cast<ZRRaster*>(ptr)->regs[reg-0x100]=value;
}
extern "C" ZRRaster *zr_raster_create(ZRBoard *board,const ZRTexelBus *tex,unsigned w)
{
    if(!board || !tex || w<512 || w>1024) return nullptr;
    try {
        auto r=std::make_unique<ZRRaster>();
        r->board=board;r->width=w;r->tex=tex;r->texels.resize(0x800000);
        for(unsigned i=0;i<r->tex_color.size();i++) r->tex_color[i]=midnrun::palette_rgba(uint16_t(i));
        if(!midnrun::decode_texture_pages(tex->lanes,0x200000,r->texels.data(),r->texels.size())) return nullptr;
        for(auto &i:r->image) i.resize(w*rows);
        r->depth.assign(w*rows,1e10f);r->packet.reserve(65536);
        zr_set_pixel_io(board,r.get(),push,swap,pixel_reg);
        return r.release();
    } catch(const std::bad_alloc &) {return nullptr;}
}
extern "C" void zr_raster_destroy(ZRRaster *r) {
    if(r) zr_set_pixel_io(r->board,nullptr,nullptr,nullptr,nullptr);
    delete r;
}
extern "C" void zr_draw_geometry(const ZRRaster *r,uint32_t *pixels,unsigned stride,unsigned w,unsigned h) {
    if(!r || !pixels || w!=r->width || stride<w || h!=rows || !zr_3d_enabled(r->board)) return;
    const auto &front=r->image[r->draw^1];
    for(unsigned y=0;y<rows;y++) for(unsigned x=0;x<w;x++) {
        uint32_t px=front[y*w+x]; if(px>>24) pixels[y*stride+x]=px;
    }
}
extern "C" ZRRasterStats zr_raster_stats(const ZRRaster *r) { return r ? r->stats : ZRRasterStats{}; }

extern "C" void zr_raster_enable_midpoints(ZRRaster *r,bool enable) {
    if(!r || r->capture_motion==enable) return;
    r->capture_motion=enable;
    r->middle_ready=r->middle_valid=false;
    r->previous.clear();r->current.clear();r->building.clear();
    if(enable) {
        r->middle_image.resize(size_t(r->width)*rows);
        r->middle_depth.resize(size_t(r->width)*rows);
    }
}
extern "C" ZRMotionStats zr_raster_motion_stats(const ZRRaster *r) {
    return r ? r->motion : ZRMotionStats{};
}
extern "C" bool zr_draw_geometry_midpoint(ZRRaster *r,uint32_t *pixels,unsigned stride,unsigned w,unsigned h) {
    if(!r || !r->capture_motion || !pixels || w!=r->width || stride<w || h!=rows || !zr_3d_enabled(r->board)) return false;
    if(!r->middle_ready) {
        r->middle_ready=true;
        std::vector<Primitive> mesh;
        r->middle_valid=interpolate_geometry(r->previous,r->current,mesh,r->motion,w);
        if(r->middle_valid) {
            std::fill(r->middle_image.begin(),r->middle_image.end(),0);
            std::fill(r->middle_depth.begin(),r->middle_depth.end(),1e10f);
            auto band=[&](int first,int last) {
                ZRRasterStats scratch{};
                for(const auto &p:mesh)
                    r->rasterize(p.v[0],p.v[1],p.v[2],p.s,p.regs.data(),p.select,
                                 r->middle_image,r->middle_depth,scratch,first,last);
            };
            // The bands have disjoint pixels/depth, but retain the original
            // primitive ordering throughout each scanline.
            auto a=std::async(std::launch::async|std::launch::deferred,band,0,rows/3);
            auto b=std::async(std::launch::async|std::launch::deferred,band,rows/3,2*rows/3);
            band(2*rows/3,rows);a.get();b.get();
        }
    }
    if(!r->middle_valid) return false;
    for(unsigned y=0;y<rows;y++) for(unsigned x=0;x<w;x++) {
        auto c=r->middle_image[y*w+x];
        // The source list may have clipped away a triangle visible only
        // between the two snapshots. Reuse the nearest measured sample for
        // disocclusions without blending projected colours or the HUD.
        if(!(c>>24)) c=r->image[r->draw^1][y*w+x];
        if(c>>24) pixels[y*stride+x]=c;
    }
    return true;
}



