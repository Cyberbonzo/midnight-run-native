#include "zr107.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t load(ZRBoard *b,uint32_t a,unsigned n)
{
    uint32_t value=0; assert(zr_read(b,a,n,&value)); return value;
}
static void store(ZRBoard *b,uint32_t a,unsigned n,uint32_t value) { assert(zr_write(b,a,n,value)); }
static void adc_bit(ZRBoard *b,unsigned bit) {
    store(b,0x7e000004,1,bit?2:0); store(b,0x7e000004,1,bit?3:1); store(b,0x7e000004,1,bit?2:0);
}
static unsigned convert(ZRBoard *b,unsigned ch) {
    store(b,0x7e000004,1,4); store(b,0x7e000004,1,0);
    adc_bit(b,1); adc_bit(b,1); adc_bit(b,ch&1); adc_bit(b,ch&4); adc_bit(b,ch&2);
    unsigned v=0;
    for(unsigned i=0;i<8;i++) { adc_bit(b,0); v=(v<<1)|(load(b,0x7e000002,1)>>7); }
    store(b,0x7e000004,1,4); return v;
}
static void ee_bit(ZRBoard *b,unsigned v) {
    store(b,0x7e000003,1,4|v); store(b,0x7e000003,1,6|v); store(b,0x7e000003,1,4|v);
}
static void ee_cmd(ZRBoard *b,unsigned opcode,unsigned addr) {
    store(b,0x7e000003,1,0); store(b,0x7e000003,1,4); ee_bit(b,1);
    unsigned c=(opcode<<6)|addr;
    for(int i=7;i>=0;i--) ee_bit(b,(c>>i)&1);
}
int main(int argc,char **argv) {
    if (argc!=2) return 2;
    FILE *f=fopen(argv[1],"rb"); if(!f) return 2;
    uint8_t *rom=malloc(0x200000); assert(rom && fread(rom,1,0x200000,f)==0x200000 && fgetc(f)==EOF); fclose(f);
    ZRBoard *b=zr_create(rom,0x200000); assert(b);
    uint32_t vector=load(b,0xfffffffc,4);
    assert(vector==load(b,0x7f9ffffc,4) && vector==load(b,0x7ffffffc,4));
    assert(vector==(((uint32_t)rom[0x1ffffc]<<24)|((uint32_t)rom[0x1ffffd]<<16)|(rom[0x1ffffe]<<8)|rom[0x1fffff]));
    store(b,0x00000604,4,0x98765432); assert(load(b,0x80000605,2)==0x7654);
    store(b,0x74020032,2,0x001a); store(b,0x74002040,2,0x7846);
    assert(load(b,0x74000040,2)==0x7846 && zr_tile_page(b,14)[64]==0x78);
    store(b,0x78000040,4,0x12345678); store(b,0x780c0000,4,0x174000a5);
    assert(zr_sharc_irq(b)==3 && !zr_sharc_in_reset(b) && zr_3d_enabled(b));
    uint32_t v=0; assert(zr_sharc_read(b,0x400020,&v) && v==0x1234);
    assert(zr_sharc_read(b,0x700000,&v) && v==0xa5);
    zr_sharc_wait_for_host_write(b);
    assert(zr_sharc_waiting_for_host(b));
    store(b,0x78010004,4,0x02040608);
    assert(!zr_sharc_waiting_for_host(b));
    assert(zr_sharc_write(b,0x700001,0x12)); assert(load(b,0x780c0004,4)==0x00800012);
    zr_vblank(b); assert(zr_ppc_irq(b)==1); store(b,0x7e000004,1,0x40); assert(!zr_ppc_irq(b));
    zr_set_inputs(b,213,128,37,0x80,true);
    assert(load(b,0x7e000000,1)==0x7f && load(b,0x7e000003,1)==0xdd);
    assert(convert(b,0)==213 && convert(b,1)==128 && convert(b,2)==37);
    ee_cmd(b,0,0x30); ee_cmd(b,1,17);
    for(int i=15;i>=0;i--) ee_bit(b,(0x695au>>i)&1);
    ee_cmd(b,2,17);
    unsigned word=0; for(int i=0;i<16;i++) { ee_bit(b,0); word=(word<<1)|((load(b,0x7e000004,1)>>4)&1); }
    assert(word==0x695a);
    store(b,0x7e00c000,4,0xabcd1234); assert(zr_sound_mail_read(b,1)==0xcd);
    zr_sound_mail_write(b,4,1); store(b,0x7e00c007,1,1); assert(zr_sound_irq(b));
    zr_sound_mail_write(b,4,0); assert(!zr_sound_irq(b));
    zr_sound_mail_write(b,0,0x42); assert(load(b,0x7e00c008,1)==0x42);
    assert(!zr_read(b,0x78040000,4,&v)); assert(!zr_read(b,0x12345678,4,&v));
    printf("Reset word %08x. RAM/ROM mirrors, tile banks, vblank IRQ, SHARC bank/mailboxes, 93C46, controls/ADC0838 and sound mailboxes passed.\n",vector);
    zr_destroy(b);free(rom);return 0;
}
