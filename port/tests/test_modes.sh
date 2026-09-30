#!/bin/sh
# Owner-local ROM diagnostic. All saves/screenshots are disposable under OUT.
# Uses its own Xvfb server; never sends input to the owner's desktop.
set -eu
bin=$1/maniac-mansion-port
rom=$2
out=$3
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
rom_before=$(sha256sum "$rom" | cut -d ' ' -f 1)
run() {
    name=$1; mode=$2; shift 2
    d=$out/$name
    mkdir -p "$d"
    env XDG_CONFIG_HOME="$d/config" XDG_DATA_HOME="$d/data" XDG_STATE_HOME="$d/state" SDL_AUDIODRIVER=dummy "$@" \
        xvfb-run -a -s '-screen 0 1024x768x24' "$bin" --mode "$mode" --rom "$rom" \
        --trace "$here/../traces/smoke.mmin" --frames 900 --hashes "$d/hashes.txt" \
        --window-shot "800:$d/window.png" --record-video /dev/null \
        --set video.vsync=off --set audio.enabled=off > "$d/run.log" 2>&1
}
if [ "${MM_MODE_TEST_SAVE_ONLY:-0}" != 1 ]; then
run original original MM_DEV_MENU=0
run original_stale original MM_DEV_MENU=0 MM_ENHANCEMENTS_HOLD_WALK=on MM_ENHANCEMENTS_OBJECT_SNAP=on MM_ENHANCEMENTS_QUICK_VERBS=on MM_ENHANCEMENTS_SMOOTH_DISPLAY=on MM_ENHANCEMENTS_GRAPHICS_SAMPLE=on MM_ENHANCEMENTS_KID_SWITCHING=on
run enhanced_off enhanced MM_DEV_MENU=0
cmp "$out/original/hashes.txt" "$out/original_stale/hashes.txt"
cmp "$out/original/window.png" "$out/original_stale/window.png"
cmp "$out/original/hashes.txt" "$out/enhanced_off/hashes.txt"
cmp "$out/original/window.png" "$out/enhanced_off/window.png"
echo 'PASS Original ignores stale enhanced settings; Enhanced OFF matches Original (900 native hashes + window pixels)'
fi
# Preserve even an existing Original battery file while using Enhanced.
d=$out/save_isolation
mkdir -p "$d/data/maniac-mansion-port/saves"
python3 - "$d" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])/'data/maniac-mansion-port/saves/maniac-mansion-usa.sav'
p.write_bytes(bytes(range(256))*32)
(p.parent/'original-before.bin').write_bytes(p.read_bytes())
PY
save_run() {
    mode=$1; keys=$2
    env XDG_CONFIG_HOME="$d/config" XDG_DATA_HOME="$d/data" XDG_STATE_HOME="$d/state" \
        SDL_AUDIODRIVER=dummy MM_DEV_MENU=1 MM_DEV_KEYS="$keys" \
        xvfb-run -a -s '-screen 0 1024x768x24' "$bin" --mode "$mode" --rom "$rom" \
        --frames 30 --record-video /dev/null --set video.vsync=off --set audio.enabled=off \
        > "$d/$mode.log" 2>&1
}
save_run enhanced '10:F11'
cmp "$d/data/maniac-mansion-port/saves/maniac-mansion-usa.sav" "$d/data/maniac-mansion-port/saves/original-before.bin"
python3 - "$d" <<'PY'
from pathlib import Path
import sys
base=Path(sys.argv[1])/'data/maniac-mansion-port'
original=base/'quicksave/slot1.mmst'
slots=[p for p in base.rglob('slot1.mmst') if p!=original]
assert len(slots)==1, slots
original.parent.mkdir(parents=True,exist_ok=True)
original.write_bytes(slots[0].read_bytes())
PY
save_run original '10:F12'
grep -q 'QUICK SAVE IS FROM ANOTHER MODE/BUILD' "$d/original.log"
save_run enhanced '10:F12'
grep -q 'quick load done' "$d/enhanced.log"
test "$rom_before" = "$(sha256sum "$rom" | cut -d ' ' -f 1)"
echo 'PASS Enhanced preserves Original battery; copied enhanced quick state rejected by Original; own enhanced state loads; ROM unchanged'
