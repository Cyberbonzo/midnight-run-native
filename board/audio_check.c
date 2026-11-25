#include "zr107.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path,size_t n)
{
    FILE *f=fopen(path,"rb"); if(!f)return NULL;
    uint8_t *p=malloc(n);
    if(!p || fread(p,1,n,f)!=n || fgetc(f)!=EOF) { free(p);p=NULL; }
    fclose(f);return p;
}
static void reg(ZRBoard *b,unsigned chip,unsigned r,unsigned val)
{ assert(zr_audio_write(b,0x200000u+r*2+chip,1,val)); }
int main(int argc,char **argv)
{
    if(argc!=4)return 2;
    uint8_t *program=read_file(argv[1],0x200000),*sound=read_file(argv[2],0x20000),*samples=read_file(argv[3],0x600000);
    assert(program && sound && samples);
    ZRBoard *b=zr_create(program,0x200000);
    assert(zr_set_soundrom(b,sound,0x20000) && zr_set_samples(b,samples,0x600000));
    uint32_t x=0;
    assert(zr_audio_read(b,0,4,&x) && x==0x103e00);
    assert(zr_audio_read(b,4,4,&x) && x==0x1400);
    assert(zr_audio_write(b,0x100000,4,0x12345678) && zr_audio_read(b,0x100002,2,&x) && x==0x5678);
    assert(zr_audio_write(b,0x400001,1,0x87) && zr_read(b,0x7e00c008,1,&x) && x==0x87);
    for(unsigned chip=0;chip<2;chip++) {
        reg(b,chip,0x22e,4);reg(b,chip,0x22f,0x10);
        for(unsigned i=0;i<64;i++) assert(zr_audio_read(b,0x200000+0x22d*2+chip,1,&x) && x==samples[4*0x20000+i]);
        reg(b,chip,0x22e,0x80);reg(b,chip,0x22d,0xaa+chip);reg(b,chip,0x22e,0x80);
        assert(zr_audio_read(b,0x200000+0x22d*2+chip,1,&x) && x==0xaa+chip);
        reg(b,chip,0x22e,0x80);reg(b,chip,0x22d,0);
        reg(b,chip,2,1);reg(b,chip,0x05,chip?0x11:0x1f);reg(b,chip,4,255);reg(b,chip,6,8);
        reg(b,chip,0x22f,0x21);reg(b,chip,0x214,1);
    }
    reg(b,0,0x227,0); assert(zr_audio_write(b,0x500001,1,1));
    int16_t out[400];zr_audio_render(b,out,200);
    /* PCM8 at unit pitch, two voices hard panned; the first nonzero byte
       in the original sample ROM is 0x0a at address 1. */
    assert(out[0]==480 && out[1]==480 && out[2]==1248 && out[3]==1248);
    assert(zr_audio_irq_level(b)==2);
    assert(zr_audio_write(b,0x500001,1,0) && !zr_audio_irq_level(b));
    printf("68k reset vectors %08x:%08x, 2x PCM ROM readback, reverb RAM, register lanes, mailbox, stereo samples [%d,%d,%d,%d] and IRQ2 verified.\n",0x103e00,0x1400,out[0],out[1],out[2],out[3]);
    zr_destroy(b);free(program);free(sound);free(samples);
    return 0;
}
