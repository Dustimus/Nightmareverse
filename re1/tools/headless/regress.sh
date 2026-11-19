#!/bin/bash
# regress.sh [out file]: one checkpoint per room (the first walk/ckpt_*.sav saved in each room),
# loaded and left ~20 s; one line per room: scenes loaded, whether the walk reached its mark,
# falls through the floor, RE1 enemies (type/state), Link's scene animations, where he ended,
# and warnings (a scene that held him too long, crashes, unknown script ops). Compare two runs'
# files line by line to see what a change did.
H=${NV_HEADLESS:?set NV_HEADLESS to the work folder}
OUT=${1:-$H/regress.txt}; : > "$OUT"
cd "$H/walk" || exit 1
for f in ckpt_*.sav; do
  printf "%s %s\n" "$(dd if="$f" bs=1 skip=4 count=32 2>/dev/null | tr -d '\0' | sed 's/re1_//;s/_scene//')" "${f#ckpt_}"
done | sort -k1,1 -u > "$H/regress_list.txt"
while read -r room ck; do
  CKPT=${ck%.sav} timeout 120 "$(dirname "$0")/walk.sh" "r_$room" resume 32 "wait:400;list;mark:1" NV_GODMODE=1 >/dev/null 2>&1
  L=$H/walk/r_$room/soh.log
  scene=$(grep -a -c "Scene Init" "$L")
  mark=$(grep -a -c "NVMARK 1" "$L")
  en=$(grep -a "NVENEMY type" "$L" | awk '{print $3"/"$11}' | sort | uniq -c | awk '{printf "%sx%s ",$1,$2}')
  fell=$(grep -a "NVPOS" "$L" | awk '{split($5,p,","); if (p[2]+0 < -1500) n++} END{print n+0}')
  anims=$(grep -a -c "NVCAST Link plays" "$L")
  warn=$(grep -a -E "NVMF the room|NVCAST the scene|Segmentation|Assertion|NVSCD unknown|unhandled" "$L" | sort -u | head -2 | tr '\n' '|')
  last=$(grep -a "NVPOS" "$L" | tail -1 | awk '{print $2,$4}')
  echo "$room ${ck%.sav} scenes=$scene mark=$mark fell=$fell enemies=[$en] anims=$anims $last $warn" >> "$OUT"
done < "$H/regress_list.txt"
echo DONE >> "$OUT"
