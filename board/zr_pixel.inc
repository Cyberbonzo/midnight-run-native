/* ZR107 K001005 pixel-engine registers; addresses count DSP words. */
static bool pixel_read(ZRBoard *b,unsigned r,uint32_t *value)
{
    struct ZRPixel *p=&b->pixel; *value=0;
    switch(r) {
    case 0: *value=p->fifo[p->rd]>>16;break;
    case 1:
        *value=p->fifo[p->rd]&65535;
        if(p->status==1 || p->status==2 || p->rd>=0x3ff) b->dsp_flags|=2;
        else b->dsp_flags&=~2u;
        p->rd=(p->rd+1)&0x7ff;break;
    case 0x11b: *value=0x8002;break;
    case 0x11c: *value=0x8000;break;
    case 0x11f: {
        unsigned addr=p->ram_ptr&0x3fffff;
        if(addr>=0x140000) return false;
        *value=p->ram[p->ram_ptr>=0x400000][addr];
        p->ram_ptr++;break;
    }
    }
    return true;
}
static bool pixel_write(ZRBoard *b,unsigned r,uint32_t value)
{
    struct ZRPixel *p=&b->pixel;
    switch(r) {
    case 0:
        if(p->status==1 || p->status==2 || p->wr<0x400) b->dsp_flags|=2;
        else b->dsp_flags&=~2u;
        p->fifo[p->wr]=value;p->wr=(p->wr+1)&0x7ff;
        if(p->push) p->push(p->user,value);
        break;
    case 0x11a:
        p->status=value;p->rd=p->wr=0;
        if(value==2 && p->swap) p->swap(p->user);
        break;
    case 0x11d: p->rd=p->wr=0;break;
    case 0x11e: p->ram_ptr=value;break;
    case 0x11f: {
        unsigned addr=p->ram_ptr&0x3fffff;
        if(addr>=0x140000) return false;
        p->ram[p->ram_ptr>=0x400000][addr]=(uint16_t)value;
        p->ram_ptr++;break;
    }
    default:
        if(r>=0x100 && r<=0x119) {
            p->regs[r-0x100]=value;
            if(p->reg_write) p->reg_write(p->user,r,value);
        }
    }
    return true;
}
void zr_set_pixel_io(ZRBoard *b,void *user,ZRPixelPush push,ZRPixelSwap swap,ZRPixelReg reg_write)
{
    b->pixel.user=user;b->pixel.push=push;b->pixel.swap=swap;b->pixel.reg_write=reg_write;
}
