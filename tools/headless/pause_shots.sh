#!/bin/bash
# pause_shots.sh <SoH folder> <out dir>: boot Link's house, open the pause menu and screenshot
# each of its four sides (the R key turns the box). Needs Xvfb, xdotool, ImageMagick; the folder
# as re1/tools/headless/soh_run.sh has it. For pause-menu mock-ups (NV_PAUSE_MENU.md).
R=$1; OUT=$2; D=79
cd "$R" || exit 1
export SHIP_HOME="$R" LIBGL_ALWAYS_SOFTWARE=1
Xvfb :$D -screen 0 1280x720x24 >/dev/null 2>&1 & XP=$!; sleep 2; export DISPLAY=:$D
NV_BOOT_ENTRANCE=0x0BB NV_WALK="wait:60;start;wait:2000" ./soh.elf > "$R/soh.log" 2>&1 & SP=$!
sleep 18
W=$(xdotool search --onlyvisible --name "." 2>/dev/null | tail -1)
for i in 0 1 2 3; do
  import -window root "$OUT/page$i.png"
  xdotool windowfocus --sync "$W" 2>/dev/null; xdotool keydown --window "$W" r; sleep 0.3; xdotool keyup --window "$W" r
  sleep 3
done
kill $SP; sleep 1; kill -9 $SP 2>/dev/null; kill $XP
