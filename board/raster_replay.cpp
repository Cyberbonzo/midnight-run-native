/* Reproduce an observed native-resolution frame from hardware bus writes.
   All binary inputs are private validation artifacts. */
#include "zr_raster.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>

static std::vector<uint8_t> read_file(const std::string &name)
{
    std::ifstream f(name,std::ios::binary);
    if(!f) throw std::runtime_error(name);
    return {std::istreambuf_iterator<char>(f),{}};
}
static uint32_t be(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|(p[2]<<8)|p[3]; }
static void snapshot(ZRBoard *b,const std::vector<uint8_t>&s)
{
    if(s.size()!=147520) throw std::runtime_error("tile snapshot size");
    for(unsigned i=0;i<=16;i++){
        zr_write(b,0x74020000,2,i==16?2:0);
        zr_write(b,0x74020032,2,((i&12)<<1)|(i&3));
        for(unsigned j=0;j<0x2000;j+=4) zr_write(b,0x74000000+j,4,be(&s[i*0x2000+j]));
    }
    unsigned at=17*0x2000;
    for(unsigned j=0;j<64;j+=4) zr_write(b,0x74020000+j,4,be(&s[at+j]));
    at+=64;
    for(unsigned j=0;j<8192;j+=4) zr_write(b,0x74080000+j,4,be(&s[at+j]));
}
static void ppm(const std::string &name,const std::vector<uint32_t>&px,unsigned width)
{
    std::ofstream f(name,std::ios::binary);f<<"P6\n"<<width<<" 384\n255\n";
    for(auto c:px){ f.put(char(c>>16));f.put(char(c>>8));f.put(char(c)); }
}
int main(int argc,char **argv)
{
    if(argc!=5) return 2;
    try {
        std::string assets=argv[1], folder=argv[2],trace=argv[3];unsigned width=std::atoi(argv[4]);
        std::vector<uint8_t> rom=read_file(assets+"/program.bin"),tiles=read_file(assets+"/tiles.bin");
        ZRBoard *b=zr_create(rom.data(),rom.size());
        if(!zr_set_tiles(b,tiles.data(),tiles.size())) return 2;
        auto snap=read_file(folder+"/tilesnapshot.bin");
        snapshot(b,snap);
        std::vector<uint8_t> data[4];const uint8_t *lanes[4];
        const char *names[4]={"477a13.9h","477a14.7h","477a15.5h","477a16.2h"};
        for(unsigned i=0;i<4;i++){data[i]=read_file(assets+"/"+names[i]);lanes[i]=data[i].data();}
        ZRTexelBus t{};if(!zr_texel_init(&t,lanes,0x200000))return 3;
        zr_set_video_io(b,&t,zr_texel_read,zr_texel_write);
        ZRRaster *r=zr_raster_create(b,&t,width);if(!r)return 3;
        zr_write(b,0x780c0000,4,0x10400000);
        auto events=read_file(trace);
        if(events.size()<16 || events.size()%16 || be(&events[events.size()-16])!=3) return 4;
        const unsigned final=be(&events[events.size()-12]);
        for(size_t i=0;i<events.size();i+=16) {
            const uint8_t *p=&events[i];
            unsigned type=be(p),a=be(p+4),v=be(p+8),m=be(p+12);
            switch(type) {
            case 1: if(!zr_sharc_write(b,a,v)) return 5;break;
            case 2:
                if(m==0xffffffff) zr_write(b,a,4,v);
                else if(m==0xffff0000) zr_write(b,a,2,v>>16);
                else if(m==0x0000ffff) zr_write(b,a+2,2,v);
                else for(unsigned j=0;j<4;j++) if(m&(0xff000000u>>(8*j))) zr_write(b,a+j,1,(v>>(24-8*j))&255);
                break;
            case 3: if(a+3>=final) {
                std::vector<uint32_t> px(width*384);
                zr_clear_frame(b,px.data(),width,width,384);
                zr_draw_tiles(b,1,px.data(),width,width,384);
                if(a==final) ppm(folder+"/layer1.ppm",px,width);
                zr_draw_geometry(r,px.data(),width,width,384);
                if(a==final) ppm(folder+"/geometry.ppm",px,width);
                zr_draw_tiles(b,0,px.data(),width,width,384);
                ppm(folder+"/replay-"+std::to_string(a)+".ppm",px,width);
            } break;
            default:return 6;
            }
        }
        auto s=zr_raster_stats(r);
        std::printf("Captured bus replay %llu words, %llu triangles, %llu fragments, %llu swaps, %llu truncated packets\n",
                    (unsigned long long)s.words,(unsigned long long)s.triangles,(unsigned long long)s.fragments,
                    (unsigned long long)s.swaps,(unsigned long long)s.rejected);
        zr_raster_destroy(r);zr_destroy(b);
    } catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 7;}
}
