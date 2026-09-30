#!/bin/sh
# Owner-local ROM diagnostic; isolated Xvfb, disposable saves/config and no assets.
set -eu
bin=$1/maniac-mansion-port
rom=$2
out=$3
mkdir -p "$out"
before=$(sha256sum "$rom" | cut -d ' ' -f 1)
run() {
    name=$1; mode=$2; keys=$3; shift 3
    d=$out/$name
    mkdir -p "$d"
    env XDG_CONFIG_HOME="$d/config" XDG_DATA_HOME="$d/data" XDG_STATE_HOME="$d/state" \
        SDL_AUDIODRIVER=dummy MM_DEV_MENU=1 MM_DEV_KEYS="$keys" "$@" \
        xvfb-run -a -s '-screen 0 1024x768x24' "$bin" --mode "$mode" --rom "$rom" \
        --frames 120 --hashes "$d/hashes.txt" --record-video /dev/null \
        --set video.vsync=off --set audio.enabled=off > "$d/run.log" 2>&1
}
run off enhanced ''
# Options -> Enhancements -> existing seven selectable rows -> Restoration.
run settings enhanced '10:F8,11:DOWN,12:DOWN,13:DOWN,14:OK,15:OK,16:DOWN,17:DOWN,18:DOWN,19:DOWN,20:DOWN,21:DOWN,22:DOWN,23:OK,24:OK,25:DOWN,26:OK,27:BACK,28:BACK,29:BACK,30:BACK,40:F11'
cmp "$out/off/hashes.txt" "$out/settings/hashes.txt"
grep -q '^restore_ed = on$' "$out/settings/config/maniac-mansion-port/config.ini"
grep -q '^restore_scumm = on$' "$out/settings/config/maniac-mansion-port/config.ini"
grep -q 'active Ed line=0 SCUMM references=0' "$out/settings/run.log"
legacy=$out/settings/data/maniac-mansion-port/quicksave/enhanced/slot1.mmst
cp "$legacy" "$out/legacy-before.mmst"
run settings enhanced '40:F11'
grep -q 'active Ed line=1 SCUMM references=1' "$out/settings/run.log"
profile=$out/settings/data/maniac-mansion-port/quicksave/enhanced/restoration-3/slot1.mmst
test -s "$profile"
cmp "$legacy" "$out/legacy-before.mmst"
cp "$profile" "$out/profile-before.mmst"
run settings enhanced '40:F12'
grep -q 'quick load done' "$out/settings/run.log"
# A copied whole-machine state from the wrong text profile must fail closed.
cp "$legacy" "$profile"
run settings enhanced '40:F12'
grep -q 'QUICK SAVE INVALID OR MISSING' "$out/settings/run.log"
cp "$out/profile-before.mmst" "$profile"
# Profiles also have independent slots when only one option is selected.
run ed enhanced '40:F11' MM_ENHANCEMENTS_RESTORE_ED=on
run credits enhanced '40:F11' MM_ENHANCEMENTS_RESTORE_SCUMM=on
test -s "$out/ed/data/maniac-mansion-port/quicksave/enhanced/restoration-1/slot1.mmst"
test -s "$out/credits/data/maniac-mansion-port/quicksave/enhanced/restoration-2/slot1.mmst"
run original original '40:F11' MM_ENHANCEMENTS_RESTORE_ED=on MM_ENHANCEMENTS_RESTORE_SCUMM=on
grep -q 'active Ed line=0 SCUMM references=0' "$out/original/run.log"
test -s "$out/original/data/maniac-mansion-port/quicksave/slot1.mmst"
test ! -d "$out/original/data/maniac-mansion-port/quicksave/enhanced"
after=$(sha256sum "$rom" | cut -d ' ' -f 1)
test "$before" = "$after"
echo 'PASS real host: OFF defaults, menu persistence, next-launch activation, three isolated text-profile slots, own load and wrong-profile rejection, Original ignores ON, ROM unchanged'
