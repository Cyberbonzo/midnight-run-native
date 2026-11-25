/* Standalone diagnostic for recorded PPC VRAM. Compile this file alone. */
#include <stdio.h>
#include "zr107.c"
int main(int argc,char **argv)
{
    if(argc<4 || argc>5) return 2;
    unsigned w=argc==5?(unsigned)atoi(argv[4]):683;
    if(w<512 || w>4096) return 2;
    uint8_t *tiles=malloc(0x100000), *dummy=calloc(1,0x200000);
    uint32_t *pixels=calloc((size_t)384*w,sizeof(uint32_t));
    if(!pixels || !dummy || !tiles) return 3;
    ZRBoard *b=zr_create(dummy,0x200000);
    FILE *f=fopen(argv[1],"rb");if(!f)return 4;
    if(fread(tiles,1,0x100000,f)!=0x100000)return 4;
    fclose(f);zr_set_tiles(b,tiles,0x100000);
    f=fopen(argv[2],"rb");if(!f)return 5;
    if(fread(b->pages,1,sizeof b->pages,f)!=sizeof b->pages ||
       fread(b->regs,1,sizeof b->regs,f)!=sizeof b->regs ||
       fread(b->palette,1,sizeof b->palette,f)!=sizeof b->palette) return 5;
    fclose(f);
    zr_clear_frame(b,pixels,w,w,384);
    zr_draw_tiles(b,1,pixels,w,w,384);
    zr_draw_tiles(b,0,pixels,w,w,384);
    f=fopen(argv[3],"wb");if(!f)return 6;
    fprintf(f,"P6\n%u 384\n255\n",w);
    for(size_t i=0;i<(size_t)384*w;i++) {
        uint32_t rgb=pixels[i];
        fputc((rgb>>16)&255,f);fputc((rgb>>8)&255,f);fputc(rgb&255,f);
    }
    fclose(f);zr_destroy(b);free(dummy);free(tiles);free(pixels);return 0;
}
