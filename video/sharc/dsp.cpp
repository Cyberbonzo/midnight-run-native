#include "dsp.h"
#include "../../board/zr107.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {
using offs_t = uint32_t;
using u32 = uint32_t;
template<typename T> constexpr T BIT(T v, unsigned off, unsigned width=1) {
    return (v >> off) & ((T(1) << width)-1);
}
namespace util {
template<typename T> constexpr T make_bitmask(unsigned width) {
    using U=std::make_unsigned_t<T>;
    return width>=sizeof(T)*8 ? T(~U(0)) : T((U(1)<<width)-1);
}
constexpr int32_t sext(uint32_t n,unsigned width) {
    if (!width) return 0;
    return static_cast<int32_t>(n<<(32-width))>>(32-width);
}
}
std::runtime_error emu_fatalerror(const char *fmt,...) {
    char msg[240]; va_list ap; va_start(ap,fmt);
    vsnprintf(msg,sizeof(msg),fmt,ap); va_end(ap);
    return std::runtime_error(msg);
}
#define fatalerror(...) throw emu_fatalerror(__VA_ARGS__)
inline int64_t mul_32x32(int32_t l,int32_t r) { return int64_t(l)*r; }
inline uint64_t mulu_32x32(uint32_t l,uint32_t r) { return uint64_t(l)*r; }
inline int32_t mul_32x32_shift(int32_t l,int32_t r,unsigned s) { return int32_t(mul_32x32(l,r)>>s); }
class adsp21062_device {
public:
#include "decl.inc"
    using NativeBody=void(adsp21062_device::*)();
    struct Entry { uint64_t bits; NativeBody run; };
    struct Cached { uint64_t bits=~uint64_t(0); NativeBody run=nullptr; };
    std::array<Cached,0xa000> cache{};
    std::array<std::array<uint32_t,0x8000>,2> ram{};
    struct { bool use=false; void select(int){use=true;} void disable(){use=false;} } m_dm_short_view;
    struct { void operator()(int) {} } m_flag_out_cb[4];
    std::string error;
    ZRBoard *board;
    sharc_internal_state *m_core;
    bool held_reset=true;
    bool waiting_for_host=false;
    bool trace=std::getenv("MIDNIGHT_DSP_TRACE")!=nullptr;
    unsigned trace_records=0;
    explicit adsp21062_device(ZRBoard *b);
    ~adsp21062_device();
    static const char *tag(){return "Midnight Run SHARC";}
    void reset();
    int run(unsigned);
    uint32_t pc() const;
    static const Entry catalog[];
};

#include "state.inc"
#include "ops.inc"
#include "pipeline.inc"
#include "dma.inc"

const adsp21062_device::Entry adsp21062_device::catalog[]={
#include "catalog.inc"
};
adsp21062_device::adsp21062_device(ZRBoard *b):board(b),m_core(new sharc_internal_state{}) {}
adsp21062_device::~adsp21062_device(){delete m_core;}
uint32_t adsp21062_device::pc() const{return m_core->pc;}

static unsigned program_bank(uint32_t a) { return a>=0x28000; }
static unsigned packed_base(uint32_t a) { return (a&0xfff)+((a&0x7fff)>>13)*0x3000; }
uint64_t adsp21062_device::pm_read48(uint32_t a) {
    if (a<0x20000 || a>=0x40000 || (a&0x7fff)>=0x5000) return 0;
    const auto &bank=ram[program_bank(a)]; unsigned base=packed_base(a);
    uint32_t lo=bank[base+0x1000], hi;
    if (a&0x1000) { hi=bank[base+0x2000]; lo&=65535; }
    else { hi=bank[base]; lo>>=16; }
    return (uint64_t(hi)<<16)|lo;
}
void adsp21062_device::pm_write48(uint32_t a,uint64_t value) {
    if (a<0x20000 || a>=0x40000 || (a&0x7fff)>=0x5000) return;
    auto &bank=ram[program_bank(a)]; unsigned base=packed_base(a);
    auto &lo=bank[base+0x1000];
    if (a&0x1000) {bank[base+0x2000]=uint32_t(value>>16); lo=(lo&0xffff0000)|uint16_t(value);}
    else {bank[base]=uint32_t(value>>16); lo=(lo&0xffff)|(uint32_t(value)<<16);}
}
uint32_t adsp21062_device::pm_read32(uint32_t a){return uint32_t(pm_read48(a)>>16);}
void adsp21062_device::pm_write32(uint32_t a,uint32_t v){pm_write48(a,(uint64_t(v)<<16)|(pm_read48(a)&65535));}
uint32_t adsp21062_device::dm_read32(uint32_t a) {
    if (a>=0x20000 && a<0x40000) return ram[a>=0x28000][a&0x7fff];
    if (a>=0x40000 && a<0x80000) {
        uint16_t value=uint16_t(ram[a>=0x50000][(a&0xffff)>>1]>>(16*(a&1)));
        return m_dm_short_view.use?uint32_t(int16_t(value)):value;
    }
    if (a==0) return m_core->syscon;
    if (a==0x37) return m_core->dma_status;
    uint32_t v=0;
    if (zr_sharc_read(board,a,&v)) {
        if (a==0x600001) m_core->flag[1]=(zr_sharc_flags(board)>>1)&1;
        return v;
    }
    throw emu_fatalerror("Unmapped read DM[%08x] at PC %05x",a,pc());
}
void adsp21062_device::dm_write32(uint32_t a,uint32_t v) {
    if(trace && trace_records<24000 && ((a>=0x28049 && a<=0x2804a) || a==0x700001 || a==0x700000)) {
        std::fprintf(stderr,"dspbus pc=%05x a=%06x v=%08x irq=%x/%x active=%d mode=%x q=%08x,%08x\n",
            pc(),a,v,m_core->irq_pending,m_core->imask,m_core->interrupt_active,m_core->mode1,
            ram[1][0x49],ram[1][0x4a]);
        trace_records++;
    }
    if (a>=0x20000 && a<0x40000) {ram[a>=0x28000][a&0x7fff]=v;return;}
    if (a>=0x40000 && a<0x80000) {
        auto &r=ram[a>=0x50000][(a&65535)>>1];
        unsigned shift=16*(a&1); r=(r&~(0xffffu<<shift))|(uint32_t(uint16_t(v))<<shift);return;
    }
    if (a==0) { m_core->syscon=v; return; }
    if (a==2 || (a>=8 && a<=15) || a==0x14 || a==0x17 || a==0x20) return;
    if (a==0x1c || a==0x1d) {
        unsigned channel=a-0x16;
        m_core->dma[channel].control=v;
        if(v&1) sharc_dma_exec(channel);
        return;
    }
    if(a>=0x40 && a<=0x4f) {
        auto &channel=m_core->dma[6+(a-0x40)/8];
        switch(a&7){
          case 0:channel.int_index=v;break; case 1:channel.int_modifier=v;break;
          case 2:channel.int_count=v;break; case 3:channel.chain_ptr=v;break;
          case 4:channel.gen_purpose=v;break; case 5:channel.ext_index=v;break;
          case 6:channel.ext_modifier=v;break; case 7:channel.ext_count=v;break;
        }
        return;
    }
    if (zr_sharc_write(board,a,v)) {
        if (a==0x600000 || a==0x700001) m_core->flag[1]=(zr_sharc_flags(board)>>1)&1;
        if (a==0x700000 || a==0x700001) m_core->flag[0]=zr_sharc_flags(board)&1;
        if (a==0x700001 && (v&3)) {
            m_core->irq_pending|=1u<<6;
            zr_sharc_ack_irq(board,4);
        }
        // The diagnostic probes a second FIFO with no consumer. Hold the DSP
        // until the PPC finishes the timeout and releases the shared bus.
        if(a==0x600000 && pc()==0x201e6 && pm_read48(pc())==0x110000600000ULL) {
            zr_sharc_wait_for_host_write(board);
            waiting_for_host=true;
        }
        return;
    }
    throw emu_fatalerror("Unmapped write DM[%08x]=%08x at PC %05x",a,v,pc());
}
void adsp21062_device::reset() {
    *m_core={};
    waiting_for_host=false;
    for(auto &bank:ram) bank.fill(0);
    for(auto &slot:cache) slot=Cached{};
    auto &boot=m_core->dma[6];
    boot.int_index=0x20000; boot.int_modifier=1; boot.int_count=0x100;
    boot.ext_index=0x400000; boot.ext_modifier=1; boot.ext_count=0x600; boot.control=0x2a1;
    for(unsigned i=0;i<256;i++) {
        uint64_t w=0;
        for(unsigned n=0;n<6;n++) w|=uint64_t(dm_read32(0x400000+i*6+n)&255)<<(8*n);
        pm_write48(0x20000+i,w);
    }
#include "reset.inc"
}
int adsp21062_device::run(unsigned budget) {
    if (zr_sharc_in_reset(board)) {held_reset=true; return 0;}
    if (held_reset) {reset(); held_reset=false;}
    if(waiting_for_host) {
        if(zr_sharc_waiting_for_host(board)) return 0;
        waiting_for_host=false;
    }
    unsigned irq=zr_sharc_irq(board);
    for (unsigned i=0;i<3;i++) if(irq&(1u<<i)) m_core->irq_pending|=1u<<(8-i);
    if (irq) zr_sharc_ack_irq(board,irq);
    for (unsigned i=0;i<4;i++) m_core->flag[i]=(zr_sharc_flags(board)>>i)&1;
    if(m_core->irq_pending){check_interrupts();m_core->idle=0;}
    unsigned executed=0;
    while(executed<budget && !waiting_for_host) {
        if (m_core->dma_status) {
            dma_run_cycle(6);
            dma_run_cycle(7);
        }
        if (m_core->idle) {
            if (m_core->irq_pending) {check_interrupts();m_core->idle=0;}
            else if(m_core->dma_status){executed++;continue;}
            else break;
        }
        m_core->pc=m_core->daddr; m_core->daddr=m_core->faddr;
        m_core->faddr=m_core->nfaddr++;
        m_core->astat_old_old_old=m_core->astat_old_old;
        m_core->astat_old_old=m_core->astat_old; m_core->astat_old=m_core->astat;
        uint32_t addr=pc();
        if(addr<0x20000 || addr>=0x40000 || (addr&0x7fff)>=0x5000)
            throw emu_fatalerror("Invalid SHARC PC %08x",addr);
        auto &slot=cache[program_bank(addr)*0x5000+(addr&0x7fff)];
        auto op=pm_read48(addr);
        if (slot.bits!=op) {
            const Entry *it=std::lower_bound(std::begin(catalog),std::end(catalog),op,
                  [](const Entry &e,uint64_t k){return e.bits<k;});
            if(it==std::end(catalog)||it->bits!=op)
                throw emu_fatalerror("No native body for PC %05x, word %012llx",addr,(unsigned long long)op);
            slot={op,it->run};
        }
#include "loop.inc"
        (this->*slot.run)();
        if(m_core->systemreg_latency_cycles>0 && !--m_core->systemreg_latency_cycles)
            systemreg_write_latency_effect();
        if(m_core->irq_pending) check_interrupts();
        executed++;
    }
    return int(executed);
}
}

struct ZRDSP:adsp21062_device{using adsp21062_device::adsp21062_device;};
ZRDSP *zr_dsp_create(ZRBoard *b){try{return b?new ZRDSP(b):nullptr;}catch(...){return nullptr;}}
void zr_dsp_destroy(ZRDSP *d){delete d;}
int zr_dsp_run(ZRDSP *d,unsigned budget){
    if (!d||!d->error.empty()) return -1;
    try{return d->run(budget);}catch(const std::exception &e){d->error=e.what();return -1;}
}
const char *zr_dsp_error(const ZRDSP *d){return d?d->error.c_str():"null DSP";}
uint32_t zr_dsp_pc(const ZRDSP *d){return d?d->pc():0;}
