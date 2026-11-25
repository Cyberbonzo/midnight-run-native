/* K054539 sample formats, register behavior and timing follow the BSD-licensed
   MAME device by Olivier Galibert. Independent C implementation for ZR107. */
static const double volume[256]={
0.2500000000, 0.2343230484, 0.2196291641, 0.2058567010, 0.1929478789, 0.1808485407, 0.1695079254, 0.1588784553, 0.1489155359, 0.1395773693, 0.1308247787, 0.1226210438, 0.1149317471, 0.1077246294, 0.1009694542, 0.0946378812,
0.0887033473, 0.0831409550, 0.0779273681, 0.0730407138, 0.0684604909, 0.0641674837, 0.0601436815, 0.0563722032, 0.0528372260, 0.0495239195, 0.0464183831, 0.0435075881, 0.0407793227, 0.0382221409, 0.0358253143, 0.0335787874,
0.0314731353, 0.0294995240, 0.0276496736, 0.0259158232, 0.0242906988, 0.0227674824, 0.0213397835, 0.0200016125, 0.0187473552, 0.0175717497, 0.0164698638, 0.0154370748, 0.0144690497, 0.0135617273, 0.0127113012, 0.0119142034,
0.0111670898, 0.0104668261, 0.0098104744, 0.0091952811, 0.0086186652, 0.0080782076, 0.0075716409, 0.0070968399, 0.0066518126, 0.0062346921, 0.0058437282, 0.0054772808, 0.0051338126, 0.0048118824, 0.0045101399, 0.0042273189,
0.0039622330, 0.0037137700, 0.0034808877, 0.0032626088, 0.0030580178, 0.0028662562, 0.0026865196, 0.0025180538, 0.0023601522, 0.0022121522, 0.0020734330, 0.0019434126, 0.0018215454, 0.0017073203, 0.0016002580, 0.0014999093,
0.0014058533, 0.0013176953, 0.0012350656, 0.0011576173, 0.0010850257, 0.0010169861, 0.0009532131, 0.0008934392, 0.0008374136, 0.0007849012, 0.0007356818, 0.0006895488, 0.0006463087, 0.0006057801, 0.0005677930, 0.0005321879,
0.0004988156, 0.0004675359, 0.0004382178, 0.0004107381, 0.0003849816, 0.0003608403, 0.0003382128, 0.0003170042, 0.0002971256, 0.0002784935, 0.0002610298, 0.0002446611, 0.0002293190, 0.0002149389, 0.0002014605, 0.0001888274,
0.0001769864, 0.0001658880, 0.0001554855, 0.0001457354, 0.0001365966, 0.0001280310, 0.0001200024, 0.0001124773, 0.0001054241, 0.0000988132, 0.0000926169, 0.0000868091, 0.0000813654, 0.0000762632, 0.0000714809, 0.0000669985,
0.0000627972, 0.0000588593, 0.0000551684, 0.0000517089, 0.0000484663, 0.0000454271, 0.0000425785, 0.0000399085, 0.0000374059, 0.0000350603, 0.0000328617, 0.0000308010, 0.0000288695, 0.0000270592, 0.0000253624, 0.0000237720,
0.0000222813, 0.0000208841, 0.0000195745, 0.0000183470, 0.0000171965, 0.0000161181, 0.0000151074, 0.0000141601, 0.0000132721, 0.0000124398, 0.0000116598, 0.0000109286, 0.0000102433, 0.0000096010, 0.0000089989, 0.0000084346,
0.0000079057, 0.0000074099, 0.0000069453, 0.0000065098, 0.0000061015, 0.0000057189, 0.0000053603, 0.0000050242, 0.0000047091, 0.0000044138, 0.0000041370, 0.0000038776, 0.0000036345, 0.0000034066, 0.0000031929, 0.0000029927,
0.0000028050, 0.0000026291, 0.0000024643, 0.0000023098, 0.0000021649, 0.0000020292, 0.0000019019, 0.0000017826, 0.0000016709, 0.0000015661, 0.0000014679, 0.0000013758, 0.0000012896, 0.0000012087, 0.0000011329, 0.0000010619,
0.0000009953, 0.0000009329, 0.0000008744, 0.0000008195, 0.0000007681, 0.0000007200, 0.0000006748, 0.0000006325, 0.0000005928, 0.0000005557, 0.0000005208, 0.0000004882, 0.0000004576, 0.0000004289, 0.0000004020, 0.0000003768,
0.0000003531, 0.0000003310, 0.0000003102, 0.0000002908, 0.0000002725, 0.0000002555, 0.0000002394, 0.0000002244, 0.0000002103, 0.0000001972, 0.0000001848, 0.0000001732, 0.0000001623, 0.0000001522, 0.0000001426, 0.0000001337,
0.0000001253, 0.0000001174, 0.0000001101, 0.0000001032, 0.0000000967, 0.0000000906, 0.0000000850, 0.0000000796, 0.0000000746, 0.0000000700, 0.0000000656, 0.0000000615, 0.0000000576, 0.0000000540, 0.0000000506, 0.0000000474,
0.0000000445, 0.0000000417, 0.0000000391, 0.0000000366, 0.0000000343, 0.0000000322, 0.0000000301, 0.0000000283, 0.0000000265, 0.0000000248, 0.0000000233, 0.0000000218, 0.0000000204, 0.0000000192, 0.0000000180, 0.0000000168
};
static const double panning[15]={0.0000000000, 0.2672612419, 0.3779644730, 0.4629100499, 0.5345224838, 0.5976143047, 0.6546536707, 0.7071067812, 0.7559289460, 0.8017837257, 0.8451542547, 0.8864052604, 0.9258200998, 0.9636241117, 1.0000000000};
static unsigned u24(const uint8_t *p) { return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16); }
static uint8_t pcm_byte(const ZRBoard *b,uint32_t a)
{
    a&=0x7fffff; return b->samples && a<0x600000 ? b->samples[a] : 0xff;
}
static int pcm_clip(int value) { return value>32767?32767:value < -32768?-32768:value; }
static uint8_t pcm_read(ZRBoard *b, unsigned chip,unsigned reg)
{
    struct ZRPcm *p=&b->pcm[chip];
    if (reg!=0x22d) return p->regs[reg];
    if (!(p->regs[0x22f]&16)) return 0;
    uint32_t pos=p->cursor;
    p->cursor=(pos+1)&0x1ffff;
    return p->regs[0x22e]==0x80 ? p->ram[(pos&0x3fff)|((pos&0x10000)>>2)] : pcm_byte(b,p->regs[0x22e]*0x20000u+pos);
}
static void pcm_write(ZRBoard *b,unsigned chip,unsigned reg,uint8_t value)
{
    struct ZRPcm *p=&b->pcm[chip];
    unsigned ch=reg>>5, off=reg&31;
    bool latch=(p->regs[0x22f]&1)!=0, update=(p->regs[0x22f]&128)==0;
    if (latch && reg<0x100 && off>=12 && off<=14) {
        p->latch[ch][off-12]=value; return;
    }
    switch(reg) {
    case 0x214:
        for (ch=0;ch<8;ch++) if (value&(1<<ch)) {
            if(latch) memcpy(&p->regs[ch*32+12],p->latch[ch],3);
            if(update) p->regs[0x22c]|=1u<<ch;
        }
        break;
    case 0x215:
        if(update) p->regs[0x22c]&=(uint8_t)~value;
        break;
    case 0x227:
        p->timer_phase=0; p->timer_high=false; p->timer_programmed=true; break;
    case 0x22d: {
        unsigned pos=p->cursor;
        if(p->regs[0x22e]==0x80) p->ram[(pos&0x3fff)|((pos&0x10000)>>2)]=value;
        p->cursor=(pos+1)&0x1ffff; break;
    }
    case 0x22e: p->cursor=0; break;
    case 0x22f: if(!(value&32)) p->timer_high=false; break;
    }
    p->regs[reg]=value;
}
static bool sample_at(const ZRBoard *b,unsigned format,uint32_t pos,int previous,int *value)
{
    static const int deltas[16]={0,1,2,4,8,16,32,64,0,-64,-32,-16,-8,-4,-2,-1};
    if (format==8) {
        uint8_t byte=pcm_byte(b,pos>>1);
        if(byte==0x88) return false;
        *value=pcm_clip(previous+256*deltas[(pos&1)?byte>>4:byte&15]);return true;
    }
    uint8_t lo=pcm_byte(b,pos);
    int16_t s=format==4 ? (int16_t)(lo|(pcm_byte(b,pos+1)<<8)) : (int16_t)(lo<<8);
    if(s==INT16_MIN) return false;
    *value=s; return true;
}
static int mix_channel(const ZRBoard *b,struct ZRPcm *p,unsigned ch)
{
    uint8_t *regs=p->regs+ch*32;
    unsigned mode=p->regs[0x200+2*ch],format=mode&12;
    if(format==12) return 0;
    struct ZRVoice *v=p->voice+ch;
    uint32_t source_pos=u24(regs+12);
    if (source_pos!=v->position) {
        v->position=source_pos;v->phase=0;v->value=0;
    }
    uint32_t pos=v->position;
    int dir=(mode&0x20)?-1:1, phase=v->phase;
    if(format==8) { phase*=2;pos=pos*2+((unsigned)phase>>16);phase&=65535; }
    phase+=dir*(int)u24(regs);
    int step=dir*(format==4?2:1);
    while(phase<0 || phase>65535) {
        phase-=dir*65536; pos=(pos+step)&(format==8?0x1ffffff:0xffffff);
        if(!sample_at(b,format,pos,v->value,&v->value)) {
            bool loop=(p->regs[0x201+2*ch]&1)!=0;
            if(loop) pos=u24(regs+8)*(format==8?2u:1u);
            if(!loop || !sample_at(b,format,pos,v->value,&v->value)) {
                v->value=0;
                if(!(p->regs[0x22f]&128)) p->regs[0x22c]&=(uint8_t)~(1u<<ch);
                break;
            }
        }
    }
    if(format==8) { phase=(phase>>1)|((pos&1)<<15);pos>>=1; }
    v->position=pos;v->phase=phase;
    if(!(p->regs[0x22f]&128)) { regs[12]=(uint8_t)pos;regs[13]=(uint8_t)(pos>>8);regs[14]=(uint8_t)(pos>>16); }
    return v->value;
}
static int16_t echo_read(const struct ZRPcm *p,unsigned slot) {
    unsigned addr=(slot&0x1fff)*2; return (int16_t)(p->ram[addr]|(p->ram[addr+1]<<8));
}
static void echo_write(struct ZRPcm *p,unsigned slot,int value) {
    unsigned addr=(slot&0x1fff)*2;
    p->ram[addr]=(uint8_t)value;p->ram[addr+1]=(uint8_t)((uint16_t)value>>8);
}
static void mix_chip(ZRBoard *b,unsigned index,double *left,double *right)
{
    struct ZRPcm *p=&b->pcm[index];
    if(p->timer_programmed && (p->regs[0x22f]&32)) {
        p->timer_phase+=2*(38u+p->regs[0x227]);
        if(p->timer_phase>=14400) {
            p->timer_phase-=14400; p->timer_high=!p->timer_high;
            if(index==0 && p->timer_high && (b->sound_control&1)) b->sound_timer_irq=true;
        }
    }
    if(!(p->regs[0x22f]&1)) return;
    double l=echo_read(p,p->reverb_pos),r=l;
    echo_write(p,p->reverb_pos,0);
    for(unsigned ch=0;ch<8;ch++) if(p->regs[0x22c]&(1<<ch)) {
        int s=mix_channel(b,p,ch);
        uint8_t *reg=p->regs+ch*32;
        unsigned pan=reg[5],vol=reg[3],rv=vol+reg[4];
        if(pan>=0x81 && pan<=0x8f) pan-=0x81;
        else if(pan>=0x11 && pan<=0x1f) pan-=0x11;
        else pan=7;
        l+=s*volume[vol]*panning[pan];r+=s*volume[vol]*panning[14-pan];
        unsigned addr=p->reverb_pos*2+((reg[6]|(reg[7]<<8))>>3);
        echo_write(p,addr,echo_read(p,addr)+(int)(s*volume[rv>255?255:rv]*.5));
    }
    p->reverb_pos=(p->reverb_pos+1)&0x1fff;
    *left+=l*.75;*right+=r*.75;
}
bool zr_set_soundrom(ZRBoard *b,const uint8_t *rom,size_t size) {
    if(!rom || size!=0x20000) return false;
    b->soundrom=rom;return true;
}
bool zr_set_samples(ZRBoard *b,const uint8_t *samples,size_t size) {
    if(!samples || size!=0x600000) return false;
    b->samples=samples;return true;
}
void zr_audio_render(ZRBoard *b,int16_t *stereo,size_t frames)
{
    for(size_t i=0;i<frames;i++) {
        double l=0,r=0;mix_chip(b,0,&l,&r);mix_chip(b,1,&l,&r);
        if(stereo) { stereo[2*i]=(int16_t)pcm_clip((int)l);stereo[2*i+1]=(int16_t)pcm_clip((int)r); }
    }
}
static bool sound_io(ZRBoard *b,bool wr,uint32_t a,uint8_t *v)
{
    if (!wr && a<0x20000 && b->soundrom) { *v=b->soundrom[a];return true; }
    if (a>=0x100000 && a<0x104000) {
        if(wr) b->soundram[a-0x100000]=*v; else *v=b->soundram[a-0x100000];
        return true;
    }
    if (a>=0x200000 && a<0x200500) {
        unsigned chip=a&1, reg=(a-0x200000)>>1;
        if(wr) pcm_write(b,chip,reg,*v); else *v=pcm_read(b,chip,reg);
        return true;
    }
    if (a>=0x400000 && a<0x400020) {
        if (!(a&1)) { if(!wr) *v=0xff; return true; }
        unsigned r=(a-0x400000)>>1;
        if(wr) zr_sound_mail_write(b,r,*v);else *v=zr_sound_mail_read(b,r);
        return true;
    }
    if(wr && a==0x500001) { b->sound_control=*v;if(!(*v&1)) b->sound_timer_irq=false;return true; }
    if(wr && a>=0x580000 && a<=0x580001) return true;
    return false;
}
bool zr_audio_read(void *b,uint32_t a,unsigned n,uint32_t *out)
{
    if (!b || !out || (n!=1 && n!=2 && n!=4)) return false;
    uint32_t value=0;
    for(unsigned i=0;i<n;i++) { uint8_t v=0; if(!sound_io(b,false,(a+i)&0xffffff,&v)) return false; value=(value<<8)|v; }
    *out=value;return true;
}
bool zr_audio_write(void *b,uint32_t a,unsigned n,uint32_t value)
{
    if (!b || (n!=1 && n!=2 && n!=4)) return false;
    for(unsigned i=0;i<n;i++) { uint8_t v=(uint8_t)(value>>(8*(n-i-1))); if(!sound_io(b,true,(a+i)&0xffffff,&v)) return false; }
    return true;
}
unsigned zr_audio_irq_level(const ZRBoard *b) { return b->sound_timer_irq?2:b->sound_irq?1:0; }
