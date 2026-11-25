"""Create private build inputs from a verified Midnight Run Euro 1.11 arcade set."""
import argparse
import hashlib
from pathlib import Path
import json
import zipfile

def prepare(archive, reference, output):
    manifest = json.loads(reference.read_text())
    if len(manifest)!=14: raise ValueError("expected the fourteen ZR107 chips")
    with zipfile.ZipFile(archive) as z:
        chips = {name:z.read(name) for name in manifest}
    for name,expected in manifest.items():
        if hashlib.sha1(chips[name]).hexdigest()!=expected: raise ValueError(name+" has the wrong SHA-1")
    output.mkdir(parents=True,exist_ok=True)
    prog=bytearray(0x200000)
    for lane,name in enumerate(["476ea1a01.20u","476ea1a02.17u","476ea1a03.15u","476ea1a04.13u"]):
        prog[lane::4]=chips[name]
    (output/"program.bin").write_bytes(prog)
    cells=bytearray(0x100000)
    for i,name in enumerate(["477a11.35b","477a12.35a"]):
        cells[i::2]=chips[name]
    (output/"tiles.bin").write_bytes(cells)
    snd=chips["477a07.19l"];audio=bytearray(len(snd));audio[::2],audio[1::2]=snd[1::2],snd[::2]
    (output/"sound.bin").write_bytes(audio)
    (output/"samples.bin").write_bytes(b"".join(chips[n] for n in ("477a08.5r","477a09.3r","477a10.5n")))
    for n in ("477a13.9h","477a14.7h","477a15.5h","477a16.2h"): (output/n).write_bytes(chips[n])
    seed=output/"default.eeprom"
    if not seed.exists():
        seed.write_bytes(bytes([255])*128)
    print("Verified all 14 chips against the Euro 1.11 reference.")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    for key in ("archive","reference","output"):p.add_argument(key,type=Path)
    a=p.parse_args();prepare(a.archive,a.reference,a.output)

