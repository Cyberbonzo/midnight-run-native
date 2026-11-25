"""Generate native C++ instructions for the ZR107 audio CPU from the user's set."""
import argparse
import zlib
from pathlib import Path
import sys
import types
import zipfile

def generate(rom_zip, source_dir, out):
    # The CPU instruction translator is independent from the original PGM board.
    sys.path.insert(0, str(source_dir))
    dis = types.ModuleType("m68k_discover")
    dis.static_target = lambda op: (op.val & 0xffffff) if op.kind in ("absw", "absl", "pcdisp") else None
    sys.modules[dis.__name__] = dis
    import m68krec as rec
    with zipfile.ZipFile(rom_zip) as z:
        raw = z.read("477a07.19l")
    if len(raw) != 131072 or zlib.crc32(raw) != 0xa82c0ba1: raise ValueError("unexpected sound ROM revision")
    program = bytearray(raw)
    program[::2], program[1::2] = raw[1::2], raw[::2]
    out.mkdir(parents=True,exist_ok=True)
    (out/"sound.bin").write_bytes(program)
    word = lambda a: int.from_bytes(program[a:a+2],"big") if 0<=a<len(program)-1 else None
    chunks = []
    total = 0
    # Generate all aligned sites in ROM, including indirect jump targets.
    # Compact ROM data which decodes as an instruction is emitted, never invoked unless branched to.
    for base in range(0,len(program),0x800):
        functions=[]
        slots=[]
        for pc in range(base,base+0x800,2):
            ins=rec.I.decode(word,pc)
            if ins is None: continue
            try: code=rec.gen_single(ins)
            except rec.Unsupported: continue
            functions.extend(code); slots.append((pc,rec.fname(pc).replace("m_","s_",1)))
        if not slots: continue
        lines=['#include "m68k_rt.h"']+functions+[f'bool sound_page_{base:06x}(m68k::Ctx *c) {{','switch(c->pc) {']
        lines.extend(f'case {pc}: {name}(c); return true;' for pc,name in slots)
        lines.extend(['default: return false;', '}', '}'])
        (out/f'page_{base:06x}.cpp').write_text('\n'.join(lines)+'\n')
        chunks.append(base); total+=len(slots)
    lines=['#include "m68k_rt.h"']+[f'bool sound_page_{b:06x}(m68k::Ctx*);' for b in chunks]
    lines+=['bool sound_step(m68k::Ctx *c) {','switch(c->pc >> 11) {']+[f'case {b>>11}: return sound_page_{b:06x}(c);' for b in chunks]+['default: return false;','}', '}']
    (out/'dispatch.cpp').write_text('\n'.join(lines)+'\n')
    print(f'Sound ROM: {len(program)} bytes, reset PC {int.from_bytes(program[4:8],"big"):#x}, represented {total} instruction sites')

if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("rom",type=Path);p.add_argument("tools",type=Path);p.add_argument("output",type=Path)
    a=p.parse_args();generate(a.rom,a.tools,a.output)

