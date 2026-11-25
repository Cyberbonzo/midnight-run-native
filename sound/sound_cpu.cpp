#include "sound_cpu.h"
#include "m68k_rt.h"
#include <cstdio>
#include <stdexcept>
bool sound_step(m68k::Ctx*);
static ZRBoard *board;
static m68k::Ctx ctx;
using namespace m68k;
namespace m68k {
uint8_t* rpage[0x1000]{};
uint8_t* wpage[0x1000]{};
uint32_t io_rd(uint32_t a,int size){
    uint32_t value;
    if (!zr_audio_read(board,a,size,&value)) throw std::runtime_error("unmapped sound read");
    return value;
}
void io_wr(uint32_t a,uint32_t value,int size){
    if (!zr_audio_write(board,a,size,value)) throw std::runtime_error("unmapped sound write");
}
void sr_changed(Ctx *c,uint32_t next){ c->pc=next; }
void fatal(Ctx*,uint32_t,const char *why){ throw std::runtime_error(why); }
}
extern "C" void zrsound_reset(ZRBoard *b) {
    board=b;
    ctx={};
    ctx.sr_hi=0x2700;
    ctx.a[7]=rd32(0);
    ctx.pc=rd32(4);
}
extern "C" uint32_t zrsound_pc(){return ctx.pc;}
extern "C" int zrsound_run(ZRBoard *b,unsigned instructions) {
    board=b;
    try{
        for(unsigned i=0;i<instructions;i++) {
            unsigned irq=zr_audio_irq_level(board);
            if(irq>((ctx.sr_hi>>8)&7)){
                uint16_t sr=ctx.sr_hi | (ctx.x<<4)|(ctx.n<<3)|(ctx.z<<2)|(ctx.v<<1)|ctx.c;
                ctx.a[7]-=4; wr32(ctx.a[7],ctx.pc);
                ctx.a[7]-=2; wr16(ctx.a[7],sr);
                ctx.sr_hi=(ctx.sr_hi&0xf8ff)|(irq<<8)|0x2000;
                ctx.pc=rd32((24+irq)*4);
            }
            if (!sound_step(&ctx)) {std::fprintf(stderr,"Untranslated sound PC %06x\n",ctx.pc); return 0;}
        }
    }catch(const std::exception &e) {std::fprintf(stderr,"Sound at %06x: %s\n",ctx.pc,e.what());return 0;}
    return 1;
}
