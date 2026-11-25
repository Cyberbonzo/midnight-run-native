#ifndef MIDNIGHT_PPC403_H
#define MIDNIGHT_PPC403_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef struct {
    uint32_t r[32],spr[1024],dcr[1024],lr,ctr,cr,xer,msr,pc;
    uint64_t cycles;
    void *board;
    bool (*read)(void *,uint32_t,unsigned,uint32_t *);
    bool (*write)(void *,uint32_t,unsigned,uint32_t);
    uint32_t fault_addr, fault_word, irq_lines;
    unsigned fault, fault_size;
} Ppc403;
enum { PPC_BUDGET, PPC_MISSING_BLOCK, PPC_INSTRUCTION, PPC_READ, PPC_WRITE };
#ifdef __cplusplus
extern "C"
#endif
int ppc403_run(Ppc403 *cpu, uint32_t instruction_budget);
static inline uint32_t ppc_crcmp(uint32_t a,uint32_t b,uint32_t xer) {
    return (a<b?8u:a>b?4u:2u)|(xer>>31);
}
static inline void ppc_crfield(Ppc403 *c,unsigned f,uint32_t nibble) {
    unsigned shift=28-4*f;
    c->cr=(c->cr&~(15u<<shift))|((nibble&15u)<<shift);
}
static inline void ppc_record(Ppc403 *c,uint32_t v) {
    ppc_crfield(c,0,ppc_crcmp(v^0x80000000u,0x80000000u,c->xer));
}
static inline uint32_t ppc_add(Ppc403 *c,uint32_t a,uint32_t b,uint32_t ci) {
    uint64_t v=(uint64_t)a+b+ci;
    c->xer=(c->xer&~0x20000000u)|((uint32_t)(v>>32)<<29);
    return (uint32_t)v;
}
static inline uint32_t ppc_rot(uint32_t x,uint32_t n) {n&=31;return (x<<n)|(x>>((-n)&31));}
static inline uint64_t ppc_timebase(const Ppc403 *c) {
    uint64_t offset=((uint64_t)c->spr[285]<<32)|c->spr[284];
    return c->cycles+offset;
}
static inline void ppc_timebase_write(Ppc403 *c,uint32_t v,bool high) {
    uint64_t tb=ppc_timebase(c);
    tb=high?(((uint64_t)v<<32)|(uint32_t)tb):((tb&UINT64_C(0xffffffff00000000))|v);
    uint64_t offset=tb-c->cycles;
    c->spr[284]=(uint32_t)offset;c->spr[285]=(uint32_t)(offset>>32);
}
static inline uint32_t ppc_sprread(Ppc403 *c,unsigned s) {
    switch(s){case 1:return c->xer;case 8:return c->lr;case 9:return c->ctr;
    case 268:case 284:case 989:return (uint32_t)ppc_timebase(c);
    case 269:case 285:case 988:return (uint32_t)(ppc_timebase(c)>>32);
    default:return c->spr[s];}
}
static inline void ppc_sprwrite(Ppc403 *c,unsigned s,uint32_t v) {
    switch(s){case 1:c->xer=v;break;case 8:c->lr=v;break;case 9:c->ctr=v;break;
    case 989:case 284:ppc_timebase_write(c,v,false);break;case 988:case 285:ppc_timebase_write(c,v,true);break;
    default:c->spr[s]=v;}
}
static inline uint32_t ppc_clz(uint32_t v) {unsigned n=0;while(n<32 && !(v&0x80000000u)){v<<=1;++n;}return n;}
static inline uint32_t ppc_sraw(Ppc403 *c,uint32_t v,unsigned n) {
    n &= 63;
    unsigned k = n>31?31:n;
    uint32_t lost=n>=32?v:(v&((1u<<n)-1u));
    c->xer=(c->xer&~0x20000000u)|((uint32_t)((v>>31)&&lost)<<29);
    return (uint32_t)((int32_t)v>>k);
}
/* Set ZR107 external lines 0..2 before spending the next CPU time slice. */
static inline void ppc403_irq(Ppc403 *c,unsigned lines) {
    uint32_t active=((lines&1u)<<4)|((lines&2u)<<2)|(lines&4u);
    uint32_t mode=c->dcr[0xa0],level=0;
    for(unsigned i=0;i<3;++i)if(!(mode&(0x80000000u>>(i*2))))level|=(16u>>i);
    c->dcr[0x40]|=active&~c->irq_lines;
    c->dcr[0x40]=(c->dcr[0x40]&~level)|(active&level);
    c->irq_lines=active;
    if((c->msr&0x8000u)&&(c->dcr[0x40]&c->dcr[0x42])) {
        c->spr[26]=c->pc;c->spr[27]=c->msr;
        c->msr &= ~0x0000c830u;
        c->pc=(c->spr[0x3d6]&0xffff0000u)|0x500u;
    }
}
#endif
