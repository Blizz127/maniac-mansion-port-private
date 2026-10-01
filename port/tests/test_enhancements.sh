#!/bin/sh
# Host integration proof; ROM and outputs remain local. Trace playback deliberately
# bypasses input enhancements; this is presentation/default identity, not a chase test.
set -eu
bin=$1/maniac-mansion-port
rom=$2
out=$3
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
run() {
    name=$1; shift
    d=$out/$name
    mkdir -p "$d"
    env XDG_CONFIG_HOME="$d/config" XDG_DATA_HOME="$d/data" XDG_STATE_HOME="$d/state" SDL_AUDIODRIVER=dummy "$@" \
        xvfb-run -a -s '-screen 0 1024x768x24' "$bin" --mode enhanced --rom "$rom" --trace "$here/../traces/smoke.mmin" \
        --frames 900 --hashes "$d/hashes.txt" --record-video /dev/null --set video.vsync=off --set audio.enabled=off \
        > "$d/run.log" 2>&1
}
run off MM_DEV_MENU=0
run display MM_DEV_MENU=0 MM_ENHANCEMENTS_HOLD_WALK=on MM_ENHANCEMENTS_OBJECT_SNAP=on MM_ENHANCEMENTS_QUICK_VERBS=on MM_ENHANCEMENTS_DIRECT_WALK=on MM_ENHANCEMENTS_SMOOTH_DISPLAY=on MM_ENHANCEMENTS_GRAPHICS_SAMPLE=on
cmp "$out/off/hashes.txt" "$out/display/hashes.txt"
echo 'PASS all-OFF versus display/color enabled: 900 cartridge framebuffer hashes identical'
# Root -> Options -> Enhancements -> independent toggles; return to gameplay.
run menu MM_DEV_MENU=1 MM_DEV_KEYS='200:F8,201:DOWN,202:DOWN,203:DOWN,204:OK,205:OK,206:OK,207:DOWN,208:OK,209:DOWN,210:OK,211:DOWN,212:OK,213:DOWN,214:OK,215:DOWN,216:OK,217:DOWN,218:OK,219:BACK,220:BACK,221:BACK'
cmp "$out/off/hashes.txt" "$out/menu/hashes.txt"
config=$out/menu/config/maniac-mansion-port/config.ini
grep -q '^hold_walk = on$' "$config"
grep -q '^object_snap = on$' "$config"
grep -q '^quick_verbs = on$' "$config"
grep -q '^direct_walk = on$' "$config"
grep -q '^smooth_display = on$' "$config"
grep -q '^graphics_sample = on$' "$config"
grep -q '^kid_switching = on$' "$config"
echo 'PASS native menu integration: independent choices persist; 900 native framebuffer hashes unchanged'
# The persisted values must survive a subsequent load. Unit test separately
# checks precedence, unrelated comments, mappings and ROM path preservation.
run menu MM_DEV_MENU=0
cmp "$out/off/hashes.txt" "$out/menu/hashes.txt"
echo 'PASS persisted enhancements reload without changing native framebuffer hashes'
