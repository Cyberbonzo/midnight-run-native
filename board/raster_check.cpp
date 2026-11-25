#include "zr_raster.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
static uint32_t xy(int x,int y,bool end) {return (unsigned(x*16)&0x3fff)|((unsigned(-y*16)&0x1fff)<<16)|(end?0x8000:0);}
int main()
{
    std::vector<uint8_t> lane[4],rom(0x200000);
    const uint8_t *lanes[4];
    for(unsigned i=0;i<4;i++){lane[i].resize(0x200000);lanes[i]=lane[i].data();}
    ZRBoard *b=zr_create(rom.data(),rom.size());assert(b);
    ZRTexelBus t{};assert(zr_texel_init(&t,lanes,0x200000));
    auto r=zr_raster_create(b,&t,683);assert(r);
    auto reg=[&](unsigned off,uint32_t value){assert(zr_sharc_write(b,0x600000+off,value));};
    auto read=[&](unsigned off){uint32_t value=0;assert(zr_sharc_read(b,0x600000+off,&value));return value;};
    reg(0,0x12345678);assert(read(0)==0x1234 && read(1)==0x5678 && read(0x11b)==0x8002);
    for(unsigned bank=0;bank<2;bank++){
        reg(0x11e,(bank<<22)|0x40);reg(0x11f,0xabc0|bank);
    }
    for(unsigned bank=0;bank<2;bank++){
        reg(0x11e,(bank<<22)|0x40);assert(read(0x11f)==(0xabc0|bank));
    }
    reg(0x103,uint16_t(-342));reg(0x104,342);reg(0x106,uint16_t(-200));reg(0x105,183);
    reg(0x10d,255);reg(0x10e,255);reg(0x10f,255);reg(0x117,0xffff);
    assert(zr_write(b,0x780c0000,4,0x10400000));
    reg(0,0x80000000);reg(0,0x80000002);
    reg(0,xy(-300,-80,false));reg(0,0x3f8000ff);
    reg(0,xy(-220,-80,false));reg(0,0x3f8000ff);
    reg(0,xy(-300,-20,true));reg(0,0x3f8000ff);reg(0,0xff0000ff);
    reg(0x11a,2);
    std::vector<uint32_t> pixels(683*384,0xff000000);
    zr_draw_geometry(r,pixels.data(),683,683,384);
    assert(pixels[130*683+60]==0xffff0000u && pixels[100*683+60]==0xff000000u);
    const auto stats=zr_raster_stats(r);
    assert(stats.triangles==1 && stats.fragments>2250 && !stats.rejected);
    std::printf("Wide-native K001005 bus/pixel test: %llu fragments, %llu triangles; red fragment x60 lies beyond the left 4:3 edge x85.\n",
                 (unsigned long long)stats.fragments,(unsigned long long)stats.triangles);
    zr_raster_destroy(r);zr_destroy(b);
}

