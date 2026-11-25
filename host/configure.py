"""Generate the private game assets and native translation units from a local ROM set."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def run(script, *args):
    subprocess.run([sys.executable, str(ROOT / script), *map(str, args)], cwd=ROOT, check=True)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 host/configure.py /path/to/midnrun.zip")
    archive = Path(sys.argv[1]).resolve(strict=True)
    assets = ROOT / "private/assets"
    run("host/prepare.py", archive, ROOT / "host/chips.json", assets)
    run("cpu/lift.py", archive, "cpu/wide", "--widescreen", "--all-text", "--entry", "0xff85004c")
    run("sound/generate.py", archive, ROOT / "sound/tools", ROOT / "sound/gen")
    run("video/sharc/prepare.py", "video/sharc/upstream", "video/sharc/build", "--program", assets / "program.bin")
