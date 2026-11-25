"""Generate native ADSP-21062 instruction bodies for captured Midnight Run firmware.

Only the named algorithm sources are needed from MAME. No MAME executable,
machine, JIT, or CPU interpreter is linked into the game. Missing opcode
images stop the native runner. Private build output contains game-derived
constants and is kept beside the user's own ROM.
"""
import argparse
from pathlib import Path
import hashlib
import re
import struct
from firmware import program_words

HASHES = {
    "sharcdma.hxx": "e2a9d44b514a2c9a01d7ef291b7293acfb90bac3e64828cc0c39764133c28efd",
    "sharc.h": "e06c54e54a6a4323741ce980a18b03d81a57a184b50b66947248e79186b411b6",
    "sharcops.hxx": "617c684ba74b3a3b2cc3289f7bb8829184c01dd2165737c1bd1d2c175643d55f",
    "compute.hxx": "361a9a957f92091578ad3737739df11c5138c66139bdb0fbed44ffe6c4377c5b",
    "sharcinternal.ipp": "e8c960db87616a9b07f94b92741802ae910a9ea89afc4e6b1a22a1ffa40df80f",
    "sharc.cpp": "bfcf2cffffeee5beb133129ab2295f3825fa30fa8d01a3d5157be0d71180ecfd",
    "sharcops_table.cpp": "ab564c39326cdfc692d56033d82fd5912bf75309093f948a345a118cc178cfe2",
}
NOTICE = "// Derived from MAME, BSD-3-Clause, copyright Ville Linde.\n"

def between(text, left, right):
    return text[text.index(left):text.index(right, text.index(left))]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("upstream",type=Path)
    ap.add_argument("build",type=Path)
    ap.add_argument("catalog",type=Path,nargs="*")
    ap.add_argument("--program",type=Path,help="Original interleaved EAA 1.11 game program")
    args = ap.parse_args()
    if not args.catalog and not args.program: ap.error("Supply --program or a firmware capture")
    args.build.mkdir(parents=True,exist_ok=True)
    def load(name):
        data=(args.upstream/name).read_bytes()
        if hashlib.sha256(data).hexdigest()!=HASHES[name]: raise ValueError(name)
        return data.decode().replace("\r\n","\n")
    def put(name,s): (args.build/name).write_text(s,encoding="utf-8")
    h=load("sharc.h")
    declarations=between(h,"enum ASTAT_SHIFT","protected:")+between(h,"// ASTAT flags","struct cfuncs;")
    declarations+=between(h,"union SHARC_REG","class frontend;").replace("emu_timer *timer;","")
    declarations+=between(h,"static const uint32_t recips_mantissa","const address_space_config")
    declarations+=between(h,"inline void CHANGE_PC","// internal compiler state")
    declarations=declarations.replace("[[noreturn]] void sharcop_unimplemented() ATTR_COLD;","")
    declarations=re.sub(r"void (sharcop_[a-z0-9_]+)\(\);",r"template<uint64_t Word> void \1();",declarations)
    put("decl.inc",NOTICE+declarations)
    state=load("sharcinternal.ipp")
    state=between(state,"// constants for IEEE","// SHARC memory operations")+between(state,"// helper functions","#endif //")
    state=state.replace("emu_timer *delayed_iop_timer;","")
    put("state.inc",NOTICE+state)
    put("math.inc",load("compute.hxx"))
    dma=load("sharcdma.hxx").split("TIMER_CALLBACK_MEMBER")[0]
    dma=re.sub(r"\n\tif \(m_enable_drc\)\n\t\{[^}]+\}", "",dma)
    put("dma.inc",dma)
    op=load("sharcops.hxx")
    before,after=op.split("void adsp21062_device::sharcop_compute_dreg_dm_dreg_pm()",1)
    after="void adsp21062_device::sharcop_compute_dreg_dm_dreg_pm()"+after
    after=after[:after.index("void adsp21062_device::sharcop_unimplemented()")]
    after=re.sub(r"void adsp21062_device::(sharcop_\w+)\(\)",r"template<uint64_t Word> void adsp21062_device::\1()",after)
    after=after.replace("m_core->opcode","Word")
    put("ops.inc",(before+after).replace('#include "compute.hxx"','#include "math.inc"'))
    dev=load("sharc.cpp")
    put("pipeline.inc",NOTICE+between(dev,"void adsp21062_device::CHANGE_PC(","TIMER_CALLBACK_MEMBER")+between(dev,"void adsp21062_device::check_interrupts()","void adsp21062_device::execute_run()"))
    put("loop.inc",NOTICE+between(dev,"// handle looping","(this->*m_sharc_op"))
    put("reset.inc",NOTICE+between(dev,"m_core->pc = 0x20004;","void adsp21062_device::device_pre_save").split("if (m_enable_drc)")[0])
    table=[(int(a,16),int(b,16),fn) for a,b,fn in re.findall(r"\{\s*(0x\w+),\s*(0x\w+),\s*&adsp21062_device::(\w+)",load("sharcops_table.cpp"))]
    unique={0}
    if args.program: unique.update(program_words(args.program.read_bytes()))
    for p in args.catalog:
        if p.suffix==".txt": unique.update(int(s,16) for s in p.read_text().split())
        else: unique.update(v for v, in struct.iter_unpack(">Q",p.read_bytes()))
    body=[]
    for word in sorted(unique):
        fn=next((f for mask,test,f in table if (word>>32)&mask==test),None)
        if fn and fn!="sharcop_unimplemented":
            body.append(f"{{0x{word:012x}ULL, &adsp21062_device::{fn}<0x{word:012x}ULL>}},")
    put("catalog.inc","\n".join(body))
    print(f"{len(body)} statically bound DSP instruction variants")

if __name__=="__main__": main()
