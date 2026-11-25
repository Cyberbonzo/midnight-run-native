/* Konami ZR107 K056832 layers. Inputs are host-independent big-endian bus
   images. Tile pixels are decoded once from the source ROM at load time. */
static void decode_tiles(ZRBoard *b)
{
    static const unsigned bitplanes[8] = {7,3,5,1,6,2,4,0};
    for(unsigned base=0;base<0x100000;base+=8) {
        for(unsigned x=0;x<8;x++) {
            uint8_t pen=0;
            for(unsigned bit=0;bit<8;bit++)
                pen|=((b->tiles[base+bitplanes[bit]]>>(7-x))&1)<<(7-bit);
            b->tile_pixels[base+x]=pen;
        }
    }
}
static uint16_t tile_word(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }
static unsigned tile_wrap(int32_t value,unsigned span)
{
    int32_t n=value%(int32_t)span; return (unsigned)(n<0?n+(int32_t)span:n);
}
static uint32_t tile_rgb(uint16_t color)
{
    unsigned r=(color>>10)&31,g=(color>>5)&31,b=color&31;
    r=(r<<3)|(r>>2);g=(g<<3)|(g>>2);b=(b<<3)|(b>>2);
    return 0xff000000u|(r<<16)|(g<<8)|b;
}
void zr_clear_frame(const ZRBoard *b,uint32_t *pixels,unsigned stride,unsigned width,unsigned height)
{
    uint32_t c=tile_rgb(tile_word(b->palette));
    if(!pixels || stride<width) return;
    for(unsigned y=0;y<height;y++) for(unsigned x=0;x<width;x++) pixels[(size_t)y*stride+x]=c;
}
static int signed_bits(unsigned v,unsigned sign)
{
    return (int)(v&(sign-1))-(int)(v&sign);
}
/* The stock panorama has hard joins where one background ends and the next
   begins. Outside the cabinet viewport a join can expose an unrelated sky.
   Continue the adjacent painted sky at that edge without changing game-space
   positions. The central image and all 3D geometry are left intact. */
static int panorama_step(const uint32_t *pixels,unsigned stride,int x,int dir)
{
    unsigned score=0,strong=0;
    for(unsigned y=0;y<112;y+=4) {
        const uint32_t *p=pixels+(size_t)y*stride+x;
        unsigned d=0;
        for(unsigned shift=0;shift<24;shift+=8)
            d+=(unsigned)abs((int)((p[0]>>shift)&255)-(int)((p[-dir]>>shift)&255));
        score+=d;
        if(d>72) strong++;
    }
    return strong>=20 ? (int)(score/84) : 0;
}
static void extend_panorama(uint32_t *pixels,unsigned stride,unsigned width,unsigned height)
{
    int inset=((int)width-512)/2;
    for(int dir=-1;dir<=1;dir+=2) {
        const int first=dir>0?inset+512:inset-1;
        const int limit=dir>0?(int)width:-1;
        for(int x=first;x!=limit;x+=dir) {
            if(panorama_step(pixels,stride,x,dir)<32) continue;
            if(x+dir!=limit && panorama_step(pixels,stride,x+dir,dir)>12) continue;
            if(panorama_step(pixels,stride,x-dir,dir)>12) continue;
            for(unsigned y=0;y<height;y++) {
                uint32_t *line=pixels+(size_t)y*stride;
                int source=x-dir;
                for(int dest=x;dest!=limit;dest+=dir) {
                    line[dest]=line[source];
                    if(source-dir>=0 && source-dir<(int)width) source-=dir;
                }
            }
            break;
        }
    }
}
bool zr_draw_tiles(const ZRBoard *b,unsigned layer,uint32_t *pixels,unsigned stride,unsigned width,unsigned height)
{
    if(!b || !b->tiles || !pixels || stride<width || height!=384 || layer>1 || !width || width>4096) return false;
    uint16_t reg[32];uint32_t colors[4096];
    for(unsigned i=0;i<32;i++) reg[i]=tile_word(&b->regs[2*i]);
    for(unsigned i=0;i<4096;i++) colors[i]=tile_rgb(tile_word(&b->palette[2*i]));
    bool associated=true;
    for(unsigned l=0;l<4;l++) if((reg[8+l]&27)==3 && (reg[12+l]&27)==3) associated=false;
    int owner[16];
    for(unsigned i=0;i<16;i++) owner[i]=-1;
    for(unsigned l=0;l<4;l++) {
        unsigned row=(reg[8+l]>>3)&3,col=(reg[12+l]>>3)&3;
        for(unsigned j=0;j<=(reg[8+l]&3);j++) for(unsigned i=0;i<=(reg[12+l]&3);i++)
            owner[(((row+j)&3)<<2)|((col+i)&3)]=(int)l;
    }
    unsigned row=(reg[8+layer]>>3)&3,col=(reg[12+layer]>>3)&3;
    unsigned rows=1+(reg[8+layer]&3),cols=1+(reg[12+layer]&3);
    unsigned scrollbank=((reg[24]>>1)&12)|(reg[24]&3);
    unsigned scroll=(reg[5]>>(2*layer))&3;
    const uint8_t *scrollbuf=b->pages[scrollbank]+layer*0x800;
    bool flipx=(reg[0]&16)!=0, flipy=(reg[0]&32)!=0;
    int off_x=(layer==1?25:29)+(flipx?signed_bits(reg[29],2048):0);
    int off_y=27+(flipy?signed_bits(reg[30],1024):0);
    int16_t dy=(int16_t)reg[16+layer];
    unsigned fbits=(reg[3]>>6)&3;
    static const unsigned fm[4]={6,4,2,0}, masks[4]={63,15,3,0};
    unsigned flips=(reg[1]>>(2*layer))&3;
    for(unsigned y=0;y<height;y++) {
        unsigned yy=tile_wrap((int)y+dy+off_y,rows*256);
        int dx=(int16_t)reg[20+layer];
        if(scroll==0 || scroll==2) {
            unsigned line=(scroll==2?yy&~7u:yy)&511;
            const uint8_t *p=scrollbuf+4*line;
            dx=(int32_t)(((uint32_t)tile_word(p)<<16)|tile_word(p+2));
        }
        if(flipy) yy=rows*256-1-yy;
        uint32_t *dst=pixels+(size_t)y*stride;
        const unsigned py=yy&7, ty=(yy>>3)&31;
        unsigned rowpage=((row+(yy>>8))&3)*4;
        const int inset=((int)width-512)/2;
        /* A single 512-wide page wraps on original hardware. Showing the wrapped
           copy outside the viewport duplicates UI and lettering. Leave those
           margins transparent so the wider geometry can show through. */
        bool centered=(cols==1 || layer==0) && inset>0;
        unsigned x0=centered?(unsigned)inset:0;
        unsigned xend=centered?x0+512:width;
        const unsigned span=cols*512;
        unsigned xx=tile_wrap((int)x0-inset+(int16_t)dx+off_x,span);
        if(flipx) xx=span-1-xx;
        for(unsigned x=x0;x<xend;) {
            unsigned page_id=rowpage+((col+(xx>>9))&3);
            const unsigned local=xx&7;
            unsigned run=flipx?local+1:8-local;
            if(run>xend-x) run=xend-x;
            if(!associated || owner[page_id]==(int)layer) {
                const uint8_t *entry=b->pages[page_id]+((ty<<6)|((xx>>3)&63))*4;
                unsigned attr=tile_word(entry),code=tile_word(entry+2)&0x3fff;
                unsigned flip=(attr>>fm[fbits])&flips;
                unsigned xmask=(flip&1)?7:0,sy=py^((flip&2)?7:0);
                const uint8_t *row_pixels=b->tile_pixels+code*64+sy*8;
                unsigned color=(attr&masks[fbits])|((attr>>2)&(63^masks[fbits]));
                unsigned palette=(layer*64+color)*16;
                for(unsigned i=0;i<run;i++) {
                    unsigned sx=(flipx?local-i:local+i)^xmask;
                    uint8_t pen=row_pixels[sx];
                    if(pen) dst[x+i]=colors[(palette+pen)&4095];
                }
            }
            x+=run;
            if(flipx) xx=xx>=run?xx-run:span-1;
            else {xx+=run;if(xx==span) xx=0;}
        }
    }
    if(layer==1 && cols>1 && width>513 && b->enabled3d)
        extend_panorama(pixels,stride,width,height);
    return true;
}
