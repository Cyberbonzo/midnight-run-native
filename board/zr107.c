/* ZR107 interfaces for native translated code. See SOURCES.md. */
#include "zr107.h"
#include <stdlib.h>
#include <string.h>

enum ADCPhase { ADC_IDLE, ADC_START, ADC_MUX, ADC_SETTLE, ADC_MSB, ADC_SECOND, ADC_LSB, ADC_DONE };
struct Adc { enum ADCPhase phase; uint8_t pins, mux, bits, sample, output, sars; };
enum EEPhase { EE_START, EE_CMD, EE_INPUT, EE_OUTPUT, EE_DONE };
struct Eeprom {
    uint16_t mem[64], shift;
    enum EEPhase phase;
    uint8_t pins, bits, address, command, output, writes;
};
struct ZRVoice { uint32_t position; int phase, value; };
struct ZRPcm {
    uint8_t regs[0x280], latch[8][3], ram[0x8000]; struct ZRVoice voice[8];
    uint32_t cursor, reverb_pos, timer_phase;
    bool timer_high, timer_programmed;
};
struct ZRPixel {
    uint16_t ram[2][0x140000]; uint32_t fifo[0x800], regs[0x1a],ram_ptr,status;
    unsigned rd,wr;
    void *user; ZRPixelPush push;ZRPixelSwap swap;ZRPixelReg reg_write;
};
struct ZRBoard {
    struct ZRPixel pixel;
    uint32_t dsp_dram[0x100000];
    const uint8_t *soundrom, *samples;
    uint8_t soundram[0x4000], sound_control;
    bool sound_timer_irq;
    struct ZRPcm pcm[2];
    const uint8_t *rom, *tiles;
    uint8_t tile_pixels[0x100000];
    uint8_t ram[0x100000], palette[0x2000], pages[17][0x2000], regs[0x40];
    uint8_t shared[2][0x10000], lan[0x2000], ppcmail[8], dspmail[8];
    uint8_t sound_cmd[4], sound_reply[2], axes[8];
    uint8_t buttons, coin, dsp_state, cg, cg_bank, led[2], motor;
    unsigned ppc_irq, dsp_irq, dsp_flags, vertical;
    bool high_count, sound_reset, sound_ien, sound_irq, dsp_reset, enabled3d, dsp_wait_host;
    struct Adc adc;
    struct Eeprom ee;
    void *video;
    ZRVideoRead read_video;
    ZRVideoWrite write_video;
};

ZRBoard *zr_create(const uint8_t *program, size_t size)
{
    if (!program || size != 0x200000) return NULL;
    ZRBoard *b = calloc(1, sizeof *b);
    if (!b) return NULL;
    b->rom = program;
    b->adc.pins = 4; b->adc.output = 1; b->adc.sars = 1;
    b->ee.output = 1;
    memset(b->ee.mem, 255, sizeof b->ee.mem);
    b->axes[0] = 128;
    b->dsp_state = 0x80; b->dsp_reset = true;
    return b;
}
void zr_destroy(ZRBoard *b) { free(b); }
static void decode_tiles(ZRBoard *b);
bool zr_set_tiles(ZRBoard *b, const uint8_t *p, size_t n)
{
    if (!p || n != 0x100000) return false;
    b->tiles = p; decode_tiles(b); return true;
}
const uint8_t *zr_workram(const ZRBoard *b) { return b->ram; }
const uint8_t *zr_tile_page(const ZRBoard *b, unsigned page) { return page < 17 ? b->pages[page] : NULL; }
const uint8_t *zr_tile_regs(const ZRBoard *b) { return b->regs; }
const uint8_t *zr_palette(const ZRBoard *b) { return b->palette; }
void zr_set_video_io(ZRBoard *b, void *user, ZRVideoRead read, ZRVideoWrite write)
{
    b->video=user; b->read_video=read; b->write_video=write;
}
void zr_set_inputs(ZRBoard *b, uint8_t wheel, uint8_t gas, uint8_t brake, uint8_t buttons, bool coin)
{
    b->axes[0] = wheel; b->axes[1] = gas > 128 ? 128 : gas;
    b->axes[2] = brake > 128 ? 128 : brake;
    b->buttons = buttons & 0xf4; b->coin = coin;
}
static void adc_pins(ZRBoard *b, uint8_t pins)
{
    struct Adc *a = &b->adc;
    bool cs = (pins & 4) != 0, clk = (pins & 1) != 0;
    unsigned di = (pins >> 1) & 1;
    if ((pins ^ a->pins) & 4) {
        a->phase = cs ? ADC_IDLE : ADC_START;
        a->sars = a->output = 1;
    }
    if (!cs && ((pins ^ a->pins) & 1)) {
        if (clk) {
            if (a->phase == ADC_START && di) {
                a->phase=ADC_MUX; a->mux=0; a->bits=0; a->sars=0;
            } else if (a->phase == ADC_MUX) {
                a->mux=(uint8_t)((a->mux<<1)|di);
                if (++a->bits == 4) a->phase=ADC_SETTLE;
            } else if (a->phase == ADC_SECOND) {
                a->phase=ADC_LSB; a->bits=1; a->sars=0;
            }
        } else if (a->phase == ADC_SETTLE) {
            unsigned ch=((a->mux>>2)&1)|((a->mux&1)<<1)|((a->mux&2)<<1);
            int n=b->axes[ch] - ((a->mux & 8) ? 0 : b->axes[ch^1]);
            a->sample=(uint8_t)(n>0?n:0); a->bits=8; a->output=0;
            a->sars=1; a->phase=ADC_MSB;
        } else if (a->phase == ADC_MSB) {
            a->output=(a->sample >> --a->bits)&1;
            if (!a->bits) a->phase=ADC_SECOND;
        } else if (a->phase == ADC_LSB) {
            a->output=(a->sample >> a->bits++)&1;
            if (a->bits==8) a->phase=ADC_DONE;
        } else if (a->phase == ADC_DONE) {
            a->output=0; a->phase=ADC_IDLE;
        }
    }
    a->pins=pins;
}
static void ee_pins(struct Eeprom *e, uint8_t pins)
{
    if (!(pins & 4) || !(e->pins & 4)) {
        e->phase=EE_START; e->output=1; e->bits=0; e->shift=0;
    }
    if ((pins & 4) && !(e->pins & 2) && (pins & 2)) {
        unsigned di=pins&1;
        if (e->phase == EE_START && di) {
            e->phase=EE_CMD; e->shift=0; e->bits=0;
        } else if (e->phase == EE_CMD) {
            e->shift=(uint16_t)((e->shift<<1)|di);
            if (++e->bits == 8) {
                e->command=e->shift>>6; e->address=e->shift&63;
                e->phase=EE_DONE; e->bits=0; e->shift=0;
                switch (e->command) {
                case 0:
                    switch (e->address>>4) {
                    case 0: e->writes=0; break;
                    case 1: e->phase=EE_INPUT; break;
                    case 2: if (e->writes) memset(e->mem,255,sizeof e->mem); break;
                    case 3: e->writes=1; break;
                    }
                    break;
                case 1: e->phase=EE_INPUT; break;
                case 2: e->phase=EE_OUTPUT; e->output=0; break;
                case 3: if (e->writes) e->mem[e->address]=65535; break;
                }
            }
        } else if (e->phase == EE_INPUT) {
            e->shift=(uint16_t)((e->shift<<1)|di);
            if (++e->bits==16) {
                if (e->writes) {
                    if (e->command==1) e->mem[e->address]=e->shift;
                    else for (unsigned i=0;i<64;i++) e->mem[i]=e->shift;
                }
                e->phase=EE_DONE;
            }
        } else if (e->phase == EE_OUTPUT) {
            e->output=(e->mem[e->address] >> (15-e->bits)) & 1;
            if (++e->bits==16) { e->address=(e->address+1)&63; e->bits=0; }
        }
    }
    e->pins=pins;
}
bool zr_eeprom_load(ZRBoard *b, const uint8_t data[128])
{
    if (!data) return false;
    for (unsigned i=0;i<64;i++) b->ee.mem[i]=(uint16_t)((data[i*2]<<8)|data[i*2+1]);
    return true;
}
void zr_eeprom_dump(const ZRBoard *b, uint8_t data[128])
{
    for (unsigned i=0;i<64;i++) { data[i*2]=(uint8_t)(b->ee.mem[i]>>8); data[i*2+1]=(uint8_t)b->ee.mem[i]; }
}
static unsigned page(const ZRBoard *b)
{
    unsigned bank=b->regs[0x33];
    return (b->regs[1]&2) ? 16 : ((bank>>1)&12)|(bank&3);
}
static unsigned reg16(const ZRBoard *b,unsigned n) { return (b->regs[n*2]<<8)|b->regs[n*2+1]; }
static bool byte_io(ZRBoard *b, bool wr, uint32_t a, uint8_t *v)
{
    uint8_t *p=NULL;
    if (a<sizeof b->ram) p=&b->ram[a];
    else if ((!wr && a>=0x7f800000 && a<0x7fa00000) || (!wr && a>=0x7fe00000)) { *v=b->rom[a&0x1fffff]; return true; }
    else if (a>=0x74000000 && a<0x74004000) p=&b->pages[page(b)][a&0x1fff];
    else if (a>=0x74020000 && a<0x74020040) p=&b->regs[a&63];
    else if (wr && a>=0x74040000 && a<0x74040008) return true; /* Unmapped setup writes in EAA 1.11, PC ff853b1c..ff853b28. */
    else if (a>=0x74080000 && a<0x74082000) p=&b->palette[a&0x1fff];
    else if (a>=0x740a0000 && a<0x740a4000 && !wr && b->tiles) {
        unsigned bank=reg16(b,0x1a)&127;
        *v=b->tiles[(bank*0x2000+(a&0x3fff))&0xfffff]; return true;
    } else if (a>=0x74060000 && a<0x74060040) {
        if (!wr) {
            *v=0;
            if (a==0x7406001c) { b->high_count=!b->high_count; *v=b->high_count?255:0; }
            if (a==0x7406001e) { b->vertical=(b->vertical+1)&511; *v=(uint8_t)(b->vertical>>2); }
        }
        return true;
    } else if (a>=0x78000000 && a<0x78020000 && (wr || a<0x78010000)) {
        if (b->cg) { if (!wr) *v=0; return true; }
        if(wr) b->dsp_wait_host=false;
        p=&b->shared[b->cg_bank][a&65535];
    } else if (a>=0x780c0000 && a<0x780c0008) {
        unsigned r=a&7;
        if (b->cg) { if (!wr) *v=0; return true; }
        if (!wr) { *v=b->dspmail[r]; if ((r&3)==1) *v|=b->dsp_state; }
        else if (!r) {
            b->cg_bank=*v&1; b->dsp_reset=(*v&16)==0;
            if(b->dsp_reset) b->dsp_wait_host=false;
            b->dsp_irq|=(*v>>1)&3; if (*v&128) b->dsp_state|=16;
        } else if (r==1) b->enabled3d=(*v&64)!=0;
        else if (r==3 || r>=4) b->ppcmail[r]=*v;
        return true;
    } else if (a>=0x7e000000 && a<0x7e004000) {
        unsigned r=a-0x7e000000;
        if (wr) {
            switch (r) {
            case 0: case 1: b->led[r]=*v; break;
            case 2: b->motor=*v; break;
            case 3: b->sound_reset=(*v&16)==0; ee_pins(&b->ee,*v&7); break;
            case 4:
                b->cg=(*v>>4)&3; if (*v&64) b->ppc_irq&=~1u;
                if (*v&128) b->ppc_irq&=~2u;
                adc_pins(b,*v&7); break;
            }
        } else {
            switch (r) {
            case 0: *v=(uint8_t)~b->buttons; break;
            case 1: *v=255; break;
            case 2: *v=b->adc.output<<7; break;
            case 3: *v=b->coin?0xdd:0xfd; break;
            case 4: *v=(b->adc.sars<<5)|(b->ee.output<<4); break;
            default: *v=0;
            }
        }
        return true;
    } else if (a>=0x7e00a000 && a<0x7e00c000) p=&b->lan[a&0x1fff];
    else if (a>=0x7e008000 && a<=0x7e008002) {
        if (!wr) *v=(a==0x7e008000 ? 8 : 0);
        else if ((a&3)==1) { if (*v&32) b->ppc_irq|=4; if (!(*v&1)) b->ppc_irq&=~4u; }
        return true;
    } else if (a>=0x7e00c000 && a<0x7e00c010) {
        unsigned r=a&7;
        if (wr) {
            if (r<4) b->sound_cmd[r]=*v;
            if (r==7 && b->sound_ien) b->sound_irq=true;
        } else *v=r<2?b->sound_reply[r]:0;
        return true;
    } else return false;
    if (wr) *p=*v; else *v=*p;
    return true;
}
bool zr_read(void *context, uint32_t a, unsigned n, uint32_t *out)
{
    ZRBoard *b=context; uint32_t value=0;
    a&=0x7fffffff;
    if (!b || !out || (n!=1 && n!=2 && n!=4)) return false;
    if (a>=0x78040000 && a<0x78040010) return b->read_video && b->read_video(b->video,a,n,out);
    for (unsigned i=0;i<n;i++) {
        uint8_t v=0;
        if (!byte_io(b,false,a+i,&v)) return false;
        value=(value<<8)|v;
    }
    *out=value; return true;
}
bool zr_write(void *context, uint32_t a, unsigned n, uint32_t value)
{
    ZRBoard *b=context;
    a&=0x7fffffff;
    if (!b || (n!=1 && n!=2 && n!=4)) return false;
    if (a>=0x78040000 && a<0x78040010) return b->write_video && b->write_video(b->video,a,n,value);
    for (unsigned i=0;i<n;i++) {
        uint8_t v=(uint8_t)(value>>(8*(n-1-i)));
        if (!byte_io(b,true,a+i,&v)) return false;
    }
    return true;
}
void zr_vblank(ZRBoard *b) { b->ppc_irq|=1; b->dsp_flags|=2; }
unsigned zr_ppc_irq(const ZRBoard *b) { return b->ppc_irq; }
uint8_t zr_sound_mail_read(ZRBoard *b,uint32_t r) { r&=7; return r<4?b->sound_cmd[r]:0; }
void zr_sound_mail_write(ZRBoard *b,uint32_t r,uint8_t v)
{
    r&=7;
    if (r<2) b->sound_reply[r]=v;
    if (r==4) { b->sound_ien=(v&1)!=0; if (!b->sound_ien) b->sound_irq=false; }
}
bool zr_sound_irq(const ZRBoard *b) { return b->sound_irq; }
bool zr_sound_in_reset(const ZRBoard *b) { return b->sound_reset; }
#include "zr_pixel.inc"
bool zr_sharc_read(ZRBoard *b,uint32_t a,uint32_t *v)
{
    if (a>=0x400000 && a<0x408000) {
        const uint8_t *p=&b->shared[b->cg_bank^1][(a-0x400000)*2];
        *v=(p[0]<<8)|p[1]; return true;
    }
    if(a>=0x500000 && a<0x600000) { *v=b->dsp_dram[a-0x500000]&65535;return true; }
    if(a>=0x600000 && a<0x700000) return pixel_read(b,a-0x600000,v);
    if (a==0x700000 || a==0x700001) {
        const uint8_t *p=b->ppcmail+(a&1)*4;
        *v=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|(p[2]<<8)|p[3];
        return true;
    }
    return false;
}
bool zr_sharc_write(ZRBoard *b,uint32_t a,uint32_t v)
{
    if (a>=0x400000 && a<0x408000) {
        uint8_t *p=&b->shared[b->cg_bank^1][(a-0x400000)*2];
        p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; return true;
    }
    if(a>=0x500000 && a<0x600000) { b->dsp_dram[a-0x500000]=v;return true; }
    if(a>=0x600000 && a<0x700000) return pixel_write(b,a-0x600000,v);
    if (a==0x700000 || a==0x700001) {
        uint8_t *p=b->dspmail+(a&1)*4;
        for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(24-i*8));
        b->dsp_flags|=1;
        if ((a&1) && (v&3)) b->dsp_irq|=4;
        return true;
    }
    return false;
}
unsigned zr_sharc_irq(const ZRBoard *b) { return b->dsp_irq; }
void zr_sharc_ack_irq(ZRBoard *b,unsigned mask) { b->dsp_irq&=~mask; }
unsigned zr_sharc_flags(const ZRBoard *b) { return b->dsp_flags; }
void zr_sharc_wait_for_host_write(ZRBoard *b) { b->dsp_wait_host=true; }
bool zr_sharc_waiting_for_host(const ZRBoard *b) { return b->dsp_wait_host; }
bool zr_sharc_in_reset(const ZRBoard *b) { return b->dsp_reset; }
bool zr_3d_enabled(const ZRBoard *b) { return b->enabled3d; }
#include "zr_pcm.inc"

#include "zr_tiles.inc"
