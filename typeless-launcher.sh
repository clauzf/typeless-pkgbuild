#!/bin/sh
# Launch Typeless with the compatibility shim from /usr/lib/typeless.
#
# The shim hides /dev/input nodes that report pointer axes (mice, touchpads,
# tablets) from the app, so its keyboard helper cannot EVIOCGRAB them and
# freeze the cursor, and it stops the uinput virtual keyboards from advertising
# BTN_* codes (which made Hyprland keep phantom pointers).
#
# See typeless-shim.c for the details; patching the shipped binary is avoided
# so upstream updates keep working.
SHIM=/usr/lib/typeless/typeless-shim.so
APP=/opt/Typeless/typeless

if [ -r "$SHIM" ]; then
  LD_PRELOAD="$SHIM${LD_PRELOAD:+:$LD_PRELOAD}"
  export LD_PRELOAD
fi

exec "$APP" "$@"
