"""Build C basic blocks from a user-supplied Midnight Run EAA PowerPC ROM."""
import argparse
import hashlib
import json
import struct
import zipfile
from collections import Counter
from pathlib import Path

LANES = ["476ea1a01.20u","476ea1a02.17u","476ea1a03.15u","476ea1a04.13u"]
CRCS = [0xea70edf2,0x1462994f,0xb770ae46,0x9644b277]
BASE = 0xff800000

def rom_offset(addr):
    physical=addr&0x7fffffff
    if 0x7f800000<=physical<0x7fa00000 or physical>=0x7fe00000:
        return physical&0x1fffff

def sx(v,bits):
    return v-(1<<bits) if v&(1<<(bits-1)) else v

def mask(b,e):
    return sum(1<<(31-i) for i in range(32) if (i-b)%32<=(e-b)%32)

def decode(w):
    """Return C statements, control-flow category and direct target information."""
    op=w>>26
    d,a,b=(w>>21)&31,(w>>16)&31,(w>>11)&31
    D,A,B=[f'c->r[{n}]' for n in (d,a,b)]
    base=A if a else '0u'
    u=w&65535
    simm=sx(u,16)
    xo=(w>>1)&1023
    imm=f'0x{simm&0xffffffff:x}u'
    rec=bool(w&1)
    def stmt(code):return code,'next',None
    def setr(dst,expr,rc=False):
        return stmt(f'{dst}={expr};'+(f' ppc_record(c,{dst});' if rc else ''))
    def branch(tgt,absolute,cond='1',pre=''):
        link='c->lr=pc+4u;' if w&1 else ''
        target=f'0x{tgt&0xffffffff:x}u' if absolute else f'pc+0x{tgt&0xffffffff:x}u'
        return f'{pre}{link}c->pc=({cond})?({target}):(pc+4u);','jump' if cond=='1' and not link else 'branch',(tgt,absolute)
    def condition():
        preds=[]
        if not d&4:preds.append('c->ctr==0' if d&2 else 'c->ctr!=0')
        if not d&16:preds.append(f'((c->cr>>{31-a})&1u)=={int(bool(d&8))}')
        return ' && '.join(preds) if preds else '1', ('--c->ctr;' if not d&4 else '')
    if op==18:return branch(sx(w&0x3fffffc,26),w&2)
    if op==16:return branch(sx(w&0xfffc,16),w&2,*condition())
    if op==19 and xo in (16,528):
        cond,pre=condition()
        register='c->lr' if xo==16 else 'c->ctr'
        link='c->lr=pc+4u;' if w&1 else ''
        return f'uint32_t target={register}&~3u; {pre}{link}c->pc=({cond})?target:pc+4u;','indirect' if cond=='1' and not link else 'indirect-fall',None
    if op==19 and xo in (50,51):
        s=26 if xo==50 else 990
        return f'c->msr=c->spr[{s+1}]; c->pc=c->spr[{s}]&~3u;','indirect',None
    if op==19 and xo==150:return stmt('/* isync: immutable compiled instruction image */')
    if op==7:return setr(D,f'{A}*{imm}')
    if op in (14,15):return setr(D,f'{base}+'+(imm if op==14 else f'0x{u<<16:x}u'))
    if op==8:return setr(D,f'ppc_add(c,~{A},{imm},1)')
    if op in (12,13):return setr(D,f'ppc_add(c,{A},{imm},0)',op==13)
    if op in (10,11) or (op==31 and xo in (0,32)):
        signed=(op==11 or op==31 and xo==0)
        right=B if op==31 else imm if signed else f'{u}u'
        flip='^0x80000000u' if signed else ''
        return stmt(f'ppc_crfield(c,{d>>2},ppc_crcmp(({A}){flip},({right}){flip},c->xer));')
    if 24<=op<=29:
        logic=('|','|','^','^','&','&')[op-24]
        return setr(A,f'{D}{logic}0x{u<<(16 if op&1 else 0):x}u',op>=28)
    if op in (20,21,23):
        m=mask((w>>6)&31,(w>>1)&31)
        expr=f'(ppc_rot({D},{B if op==23 else b})&0x{m:x}u)'
        if op==20:expr+=f'|({A}&0x{~m&0xffffffff:x}u)'
        return setr(A,expr,rec)
    if op==31 and xo in (19,83):return setr(D,'c->cr' if xo==19 else 'c->msr')
    if op==31 and xo==144:
        bits=sum(15<<(4*i) for i in range(8) if w&(1<<(12+i)))
        return stmt(f'c->cr=(c->cr&~0x{bits:x}u)|({D}&0x{bits:x}u);')
    if op==31 and xo==146:return stmt(f'c->msr={D};')
    if op==31 and xo in (131,163):
        value=(f'({D}&0x8000u)' if xo==131 else f'0x{w&0x8000:x}u')
        return stmt(f'c->msr=(c->msr&~0x8000u)|{value};')
    if op==31 and xo in (597,725):
        action=('uint32_t value; READ(ea+i,1,value); if(!(i%4))c->r[reg]=0; c->r[reg]|=value<<shift;'
                if xo==597 else 'WRITE(ea+i,1,(c->r[reg]>>shift)&255u);')
        return stmt(f'uint32_t ea={base}; for(unsigned i=0;i<{b or 32};i++){{unsigned reg=({d}+i/4)&31u,shift=24-(i%4)*8; {action}}}')
    sr=((w>>16)&31)|((w>>6)&992)
    if op==31 and xo in (339,371):return setr(D,f'ppc_sprread(c,{sr})')
    if op==31 and xo==467:return stmt(f'ppc_sprwrite(c,{sr},{D});')
    if op==31 and xo==323:return setr(D,f'c->dcr[{sr}]')
    if op==31 and xo==451:return stmt(f'c->dcr[64]&=~{D};' if sr==64 else f'c->dcr[{sr}]={D};')
    if op==31 and xo in (54,86,246,278,454,470,598,854,966,982):
        return stmt('/* cache maintenance / ordering on coherent host memory */')
    if op==31 and xo in (28,60,124,284,316,412,444,476):
        expr={28:f'{D}&{B}',60:f'{D}&~{B}',124:f'~({D}|{B})',284:f'~({D}^{B})',316:f'{D}^{B}',412:f'{D}|~{B}',444:f'{D}|{B}',476:f'~({D}&{B})'}[xo]
        return setr(A,expr,rec)
    if op==31 and xo in (26,922,954,24,536,792,824):
        expr={26:f'ppc_clz({D})',922:f'(uint32_t)(int16_t){D}',954:f'(uint32_t)(int8_t){D}',24:f'({B}&32u)?0u:({D}<<({B}&31u))',536:f'({B}&32u)?0u:({D}>>({B}&31u))',792:f'ppc_sraw(c,{D},{B})',824:f'ppc_sraw(c,{D},{b})'}[xo]
        return setr(A,expr,rec)
    if op==31 and xo in (266,10,138,40,8,136,104,235,75,11,491,459,202,234,200,232):
        ca='((c->xer>>29)&1u)'
        expr={266:f'{A}+{B}',10:f'ppc_add(c,{A},{B},0)',138:f'ppc_add(c,{A},{B},{ca})',40:f'{B}-{A}',8:f'ppc_add(c,~{A},{B},1)',136:f'ppc_add(c,~{A},{B},{ca})',104:f'0u-{A}',235:f'{A}*{B}',75:f'(uint32_t)(((int64_t)(int32_t){A}*(int32_t){B})>>32)',11:f'(uint32_t)(((uint64_t){A}*{B})>>32)',491:f'(!{B}||({A}==0x80000000u&&{B}==0xffffffffu))?0u:(uint32_t)((int32_t){A}/(int32_t){B})',459:f'{B}?{A}/{B}:0u',202:f'ppc_add(c,{A},0,{ca})',234:f'ppc_add(c,{A},~0u,{ca})',200:f'ppc_add(c,~{A},0,{ca})',232:f'ppc_add(c,~{A},~0u,{ca})'}[xo]
        return setr(D,expr,rec)
    if op==19 and xo==0:
        return stmt(f'ppc_crfield(c,{d>>2},(c->cr>>{28-4*(a>>2)})&15u);')
    if op==19 and xo in (33,129,193,225,257,289,417,449):
        l=f'(c->cr>>{31-a})';r=f'(c->cr>>{31-b})'
        expr={33:f'~({l}|{r})',129:f'{l}&~{r}',193:f'{l}^{r}',225:f'~({l}&{r})',257:f'{l}&{r}',289:f'~({l}^{r})',417:f'{l}|~{r}',449:f'{l}|{r}'}[xo]
        return stmt(f'c->cr=(c->cr&~(1u<<{31-d}))|((({expr})&1u)<<{31-d});')
    addresses={23:(4,0,0,0),55:(4,0,1,0),87:(1,0,0,0),119:(1,0,1,0),151:(4,1,0,0),183:(4,1,1,0),215:(1,1,0,0),247:(1,1,1,0),279:(2,0,0,0),311:(2,0,1,0),343:(2,0,0,1),375:(2,0,1,1),407:(2,1,0,0),439:(2,1,1,0)}
    if 32<=op<=45 or op==31 and xo in addresses:
        if op==31:width,store,update,signed=addresses[xo]; ea=f'{base}+{B}'
        else:
            width=(4,4,1,1,4,4,1,1,2,2,2,2,2,2)[op-32];store=op in (36,37,38,39,44,45);update=op&1;signed=op in (42,43);ea=f'{base}+{imm}'
        body=f'uint32_t ea={ea};'
        body+=(f' WRITE(ea,{width},{D});' if store else f' uint32_t v; READ(ea,{width},v); {D}='+('(uint32_t)(int16_t)v;' if signed else 'v;'))
        if update:body+=f' {A}=ea;'
        return stmt(body)
    if op in (46,47):
        action='READ(ea,4,c->r[i]);' if op==46 else 'WRITE(ea,4,c->r[i]);'
        return stmt(f'uint32_t ea={base}+{imm}; for(unsigned i={d};i<32;++i,ea+=4){{{action}}}')
    if op==31 and xo==1014:return stmt(f'uint32_t ea=({base}+{B})&~15u; for(unsigned i=0;i<4;i++){{WRITE(ea+i*4,4,0);}}')
    return f'c->fault_word=0x{w:08x}u; return PPC_INSTRUCTION;','unsupported',None

def lift(zip_path, out, seeds, widescreen=False, all_text=False):
    with zipfile.ZipFile(zip_path) as z:
        rom=bytearray(2<<20)
        for i,(name,crc) in enumerate(zip(LANES,CRCS)):
            info=z.getinfo(name)
            if info.CRC!=crc or info.file_size!=len(rom)//4:raise ValueError(f'{name} does not match EAA v1.11')
            rom[i::4]=z.read(info)
    bus_rom=bytes(rom)
    source_sha256=hashlib.sha256(bus_rom).hexdigest()
    if widescreen:
        import sys
        sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'video'))
        from widescreen_program import widescreen_program
        rom=widescreen_program(bytes(rom))
    out.mkdir(parents=True,exist_ok=True)
    (out/'program.bin').write_bytes(bus_rom)
    queue=[0x1ffffc,0x500]+[rom_offset(v) for v in seeds]
    # Aside from directly connected control flow, the executable stores entries in
    # function pointer tables. Register aligned callers and candidate call targets
    # from its text segment. Unreached candidate code is never executed.
    for off in range(0,0x56000,4):
        w=struct.unpack_from('>I',rom,off)[0]
        if w>>26==18 and w&3==1:
            t=off+sx(w&0x3fffffc,26)
            if 0<=t<0x56000:queue.extend((off,t))
    for w, in struct.iter_unpack('>I',rom):
        if BASE<=w<BASE+0x56000 and not w&3:queue.append(w-BASE)
    if all_text:
        # The in-ROM executable includes embedded traceback tables as well as
        # instructions. Emit all aligned candidates through the known text region;
        # unresolved words still fault if a real branch actually executes them.
        queue.extend(range(0,0x56000,4))
    seen={}; unsupported={}
    while queue:
        off=queue.pop()
        if off is None or off in seen:continue
        w=struct.unpack_from('>I',rom,off)[0]
        code,flow,target=decode(w)
        seen[off]=code,flow
        if flow in ('next','branch','indirect-fall') and off+4<len(rom):queue.append(off+4)
        if target:
            disp,absolute=target
            v=rom_offset((disp if absolute else BASE+off+disp)&0xffffffff)
            if v is not None:queue.append(v)
        if flow=='unsupported':unsupported[hex(BASE+off)]=f'{w:08x}'
    def update_file(path, text):
        if not path.exists() or path.read_text()!=text:
            path.write_text(text)
    pages={}
    for off,(code,flow) in sorted(seen.items()):pages.setdefault(off//4096,[]).append((off,code,flow))
    protos=['#include "ppc403.h"']
    switches=[]
    for pg,lines in pages.items():
        name=f'page_{pg:03x}'
        protos.append(f'int {name}(Ppc403*,uint32_t*);')
        switches.append(f'case {pg}: code={name}(c,&budget); break;')
        src=['#include "ppc403.h"','#include "access.h"',f'int {name}(Ppc403 *c,uint32_t *budget) {{', 'uint32_t pc;', 'switch(c->pc&4095u){']
        for i,(off,code,flow) in enumerate(lines):
            src+=[f'case {off&4095}: if(!*budget)return PPC_BUDGET; --*budget; ++c->cycles; pc=c->pc; {{ {code} }}']
            if flow=='next':
                src.append('c->pc=pc+4u;')
                if i+1==len(lines) or lines[i+1][0]!=off+4:src.append('return -1;')
            elif flow!='unsupported':src.append('return -1;')
        src+=['default:return PPC_MISSING_BLOCK;','}}']
        update_file(out/(name+'.c'),'\n'.join(src)+'\n')
    src=protos+['int ppc403_run(Ppc403 *c,uint32_t budget){','while(budget){ uint32_t p=c->pc&0x7fffffffu; int code;',
        'if((p<0x7f800000u || p>=0x7fa00000u) && p<0x7fe00000u)return PPC_MISSING_BLOCK;',
        'switch((p&0x1fffffu)>>12){',*switches,'default:return PPC_MISSING_BLOCK;} if(code>=0)return code;','}return PPC_BUDGET;}']
    update_file(out/'dispatch.c','\n'.join(src)+'\n')
    stats={'source_sha256':source_sha256,'widescreen':widescreen,'all_text_candidates':all_text,'bus_sha256':source_sha256,'translation_input_sha256':hashlib.sha256(rom).hexdigest(),'locations':len(seen),'pages':len(pages),'unsupported':unsupported,'flow_counts':dict(Counter(f for _,f in seen.values()))}
    (out/'coverage.json').write_text(json.dumps(stats,indent=2))
    print('Static PowerPC addresses:',len(seen),'pages:',len(pages),'unsupported:',len(unsupported))
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom',type=Path);parser.add_argument('out',type=Path);parser.add_argument('--entry',type=lambda v:int(v,0),action='append',default=[])
    parser.add_argument('--reference-pcs',type=Path, help='Optional frame PC/LR samples from the same ROM')
    parser.add_argument('--all-text', action='store_true', help='Also translate every aligned location of the main executable text area')
    parser.add_argument('--widescreen', action='store_true', help='Apply checked widescreen projection edits before translating')
    args=parser.parse_args()
    if args.reference_pcs:
        for line in args.reference_pcs.read_text().splitlines():
            for word in line.split()[1:3]:
                addr=int(word,16)
                if rom_offset(addr) is not None and addr%4==0:args.entry.append(addr)
    lift(args.rom,args.out,args.entry,args.widescreen,args.all_text)

