#!/bin/bash
# walk.sh <name> <boot> <seconds> <NV_WALK steps> [VAR=value ...]
# One walkthrough segment: its log, steps and screenshots land in $NV_HEADLESS/walk/<name>.
#   boot: a room (106 boots re1_106 on a fresh save), "warp" (a new game: RE1's opening), or
#         "resume" (the checkpoint walk/ckpt_$CKPT.sav: a save made with the "save" step,
#         copied from a segment's walk/<name>/slot.sav)
# Layout under $NV_HEADLESS: run/ is a SoH folder (soh.elf, oot.o2r, mods/re1.o2r); walk/ holds
# the segments and ckpt_*.sav. NV_HEADLESS_DISPLAY picks the X display (default 91): two
# copies (run/ and another folder via NV_HEADLESS_RUN) can run side by side on two displays.
# The steps and settings are listed in re1/README.md, "Walking the game headless".
H=${NV_HEADLESS:?set NV_HEADLESS to the work folder}
RUN=${NV_HEADLESS_RUN:-$H/run}
n=$1; room=$2; secs=$3; w=$4; shift 4
rm -rf "$H/walk/$n"; mkdir -p "$H/walk/$n"
b=re1_$room
[ "$room" = warp ] && { b=warp; rm -f "$RUN/Save/nv_re1_255.sav"; }
[ "$room" = resume ] && { b=warp; cp "$H/walk/ckpt_$CKPT.sav" "$RUN/Save/nv_re1_255.sav"; }
env NV_WALK_TIMEOUT=100 "$@" NV_BOOT_ENTRANCE=$b NV_LOG=1 NV_WALK="$w" \
  "$(dirname "$0")/soh_run.sh" "$RUN" "$secs" "$H/walk/$n" "${NV_HEADLESS_DISPLAY:-91}" >/dev/null 2>&1
cp "$RUN/soh.log" "$H/walk/$n/soh.log"
[ -f "$RUN/Save/nv_re1_255.sav" ] && cp "$RUN/Save/nv_re1_255.sav" "$H/walk/$n/slot.sav"
grep -a -E "^NV(STEP|DOOR|ITEM|AOT|EVT|MSG|TEXT|FILE|USE|SCD|FX type|SAVE|SONG|BOX|VOICE|MARK|DOORLIST|AOTLIST|SCRIPT|CAST Link)|Display Text|sceneNum:" \
  "$H/walk/$n/soh.log" | grep -v "NVSCD run" > "$H/walk/$n/steps.txt"
