#!/bin/bash
# soh_run.sh <SoH folder> <seconds> <shots folder> [X display]
# Runs SoH under Xvfb (software GL) for N seconds, pressing Return every 4 s (answers SDL message
# boxes), and screenshots the whole display every SHOT_EVERY seconds (default 10). The game's
# 640x480 window sits at +100+100 on the 1280x720 display: crop with
# `convert shot.png -crop 640x480+100+100 +repage`.
# Needs: Xvfb, xdotool, ImageMagick. The SoH folder holds soh.elf, oot.o2r and mods/re1.o2r.
R=$1; SECS=$2; OUT=$3; D=${4:-77}
cd "$R" || exit 1
export SHIP_HOME="$R" LIBGL_ALWAYS_SOFTWARE=1
Xvfb :$D -screen 0 1280x720x24 >/dev/null 2>&1 &
XP=$!; sleep 2; export DISPLAY=:$D
./soh.elf > "$R/soh.log" 2>&1 &
SP_=$!
mkdir -p "$OUT"
for i in $(seq 1 "$SECS"); do
  sleep 1
  if [ $((i % 4)) -eq 0 ]; then
    for w in $(xdotool search --onlyvisible --name "." 2>/dev/null); do
      xdotool windowfocus --sync "$w" 2>/dev/null; xdotool key --window "$w" Return 2>/dev/null
    done
  fi
  if [ $((i % ${SHOT_EVERY:-10})) -eq 0 ]; then import -window root "$OUT/shot_$(printf %04d "$i").png" 2>/dev/null; fi
  kill -0 $SP_ 2>/dev/null || break
done
kill $SP_ 2>/dev/null; sleep 1; kill -9 $SP_ 2>/dev/null; kill $XP 2>/dev/null
