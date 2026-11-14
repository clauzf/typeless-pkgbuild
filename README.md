# Typeless on Arch Linux + Hyprland (Omarchy)

Unofficial packaging for [Typeless](https://www.typeless.com/) on Arch, plus the
five runtime fixes that were needed to make it actually usable on a Wayland
desktop. Shared in case someone hits the same walls.

Upstream ships **only a Debian `.deb`** and documents nothing about Wayland, so
everything below was found by measurement, not by reading their docs.

| # | Symptom you would notice | Actual root cause | Fix |
|---|---|---|---|
| 1 | Mouse cursor **freezes / disappears** the moment Typeless runs | Its input helper `EVIOCGRAB`s the mouse and advertises phantom pointers | [`typeless-shim.c`](typeless-shim.c) loaded with `LD_PRELOAD` |
| 2 | Dictation hotkey (**Right Alt**) never triggers | Arch's `shadow` has no `/usr/bin/sg`, so upstream's `sg input` relaunch silently no-ops | udev `uaccess` rule ordered before `73-seat-late.rules` |
| 3 | An **empty bordered rectangle** floating on the desktop | Frameless 750×500 transparent overlay canvas gets a window border | Hyprland window rule |
| 4 | "Ask anything" panel (**Right Alt + Space**) still has a border | It is a *different* window: class `Typeless`, title `AI result` | second window rule, matched by title |
| 5 | **Second dictation freezes the whole UI** (nothing clickable) | Paste backend deadlocks reading the clipboard it owns | run `wl-clip-persist` (now a package dependency) |

Install: `makepkg -si` in this directory, or

```sh
git clone <this repo> typeless-pkgbuild && cd typeless-pkgbuild
makepkg -si
```

Remove with `sudo pacman -Rns typeless`.

---

## 1. Cursor freeze: the helper grabs the mouse

**Symptom.** Move the mouse while Typeless is running and the cursor stops dead.
`hyprctl devices` shows no phantom devices, but `EVIOCGRAB` is held on the
mouse's `event` node, so the compositor never receives pointer motion.

**Why.** The helper enumerates `/dev/input/event*` and grabs every node that
reports `EV_KEY` plus `KEY_A`/`KEY_ENTER`. A Logitech G304 receiver reports
keyboard usages on the *mouse* node, so the mouse passes the "is a keyboard"
test, gets grabbed, and cursor motion dies. The helper then replays events from
its own uinput device.

Second half of the same bug: that uinput virtual keyboard advertises the whole
keycode range `0x0..0x2ff`, which includes `BTN_MOUSE`. libinput classifies it as
a pointer too, so Hyprland ends up with phantom pointers named
`usb-hid-keyboard-mouse` that have no cursor image — which is what made the
cursor "disappear" rather than just stutter.

**Fix.** `typeless-shim.c`, injected through `LD_PRELOAD` by `/usr/bin/typeless`:

- intercepts `open`/`open64`/`openat`, and for `/dev/input/event*` checks
  `EVIOCGBIT(EV_REL)`/`EVIOCGBIT(EV_ABS)`; devices with pointer axes are closed
  and reported as `ENOENT`, so the helper never sees mice, touchpads or tablets;
- intercepts `ioctl` and drops `UI_SET_KEYBIT` for codes `>= 0x100`, so the
  virtual device stays keyboard-only. Pasting still works: `Ctrl+V` is `KEY_V`.

Nothing in the shipped binary is patched, so an upstream update keeps working.

**Verify.**

```sh
# keyboard grabbed, mouse free
python3 - <<'PY'
import fcntl, os
for ev in ("event2", "event7"):
    fd = os.open("/dev/input/" + ev, os.O_RDONLY)
    try:
        fcntl.ioctl(fd, 0x40044590, 0)   # EVIOCGRAB
        print(ev, "free")
    except OSError as e:
        print(ev, "grabbed by another process", e)
    finally:
        os.close(fd)
PY
hyprctl devices -j | jq -r '.mice[].name'   # no usb-hid-keyboard-mouse
```

## 2. Hotkey dead: no `sg` on Arch

**Symptom.** Right Alt does nothing. The helper starts, finds zero usable
devices, and logs nothing useful.

**Why.** Upstream expects to be started in the `input` group and self-relaunches
via `sg input …` when it is not. Arch's `shadow` package does not ship
`/usr/bin/sg`, so the relaunch fails and the helper runs without permission to
open `/dev/input/event*`.

**Fix.** Grant the seat user read access with `uaccess` instead of relying on
group membership (no re-login required, works the same way `/dev/uinput`
already works on Arch):

```
# /usr/lib/udev/rules.d/72-typeless-input-uaccess.rules
SUBSYSTEM=="input", ENV{ID_INPUT}!="", TAG+="uaccess"
```

**The filename matters.** `uaccess` is applied by the builtin in
`73-seat-late.rules`. A rule numbered after it (the obvious `99-…`) only adds a
tag that nothing ever consumes — the devices get ACLs never. `72-` sorts before
it.

**Verify.**

```sh
getfacl /dev/input/event0          # user:you:rw-
ls -l /proc/$(pgrep -f keyboard-helper)/fd | grep event
```

## 3./4. Bordered rectangles over a frameless overlay

**Symptom.** A 750×500 rectangle outline hovers over the desktop with nothing in
it; later, the Right Alt + Space answer panel is outlined too.

**Why.** Typeless renders its UI as transparent frameless Electron windows and
draws the visible pill inside them. Hyprland outlines the whole window box, so
the transparent canvas shows up as an empty frame.

They are two different windows, which is the trap:

```
Typeless-floating-bar   'Status'     750x500   ← dictation bar
Typeless                'AI result'  800x709   ← ask-anything panel
```

The second one reuses the main application's class, so a rule matching
`^Typeless-floating-bar$` does not cover it.

**Fix.** [`hypr/typeless.lua`](hypr/typeless.lua) — two rules, the second
matched by title because the class is shared with the main window:

```lua
o.window({ class = "^Typeless-floating-bar$" },
  { border_size = 0, rounding = 0, no_shadow = true, no_blur = true,
    no_dim = true, no_anim = true })

o.window({ class = "^Typeless$", title = "^AI result$" },
  { border_size = 0, rounding = 0, no_shadow = true, no_blur = true,
    no_dim = true, no_anim = true })
```

Copy that file to `~/.config/hypr/typeless.lua` and require it from
`~/.config/hypr/hyprland.lua` (Omarchy's config loads user modules explicitly,
it does not glob the directory):

```lua
require("hypr.typeless")
```

Or paste the two `o.window(...)` calls into the already-required
`~/.config/hypr/windows.lua`. Finish with `hyprctl reload` and
`hyprctl configerrors` (must print nothing — a missing module shows up there).

**Verify** without eyeballing — a border is exactly `border_size` pixels wide at
the window's left edge:

```sh
hyprctl clients -j | jq '.[] | select(.class | test("Typeless")) | {class, title, at, size}'
grim -g "<x-4>,<y> 20x1" - | magick png:- txt:- | head   # no border-coloured pixels
```

Setting the rule to `border_size = 8` makes an 8 px band appear, which is a
quick way to prove a rule matches before you set it to `0`.

Everything else in the windows (the 1 px hairline around the panel) is drawn by
Typeless's own CSS and cannot be influenced from Hyprland.

## 5. Second dictation freezes the entire app

**Symptom.** First dictation inserts fine. The second one inserts nothing and
from then on *nothing* in the app responds — not Continue, not Back, not the
window buttons. The window keeps showing a stale frame.

**Why.** Text injection goes `fcitx5 → ibus → clipboard-paste`; the first two
are unavailable (see caveats), so it pastes. Before pasting it saves the current
clipboard:

```
[insertText] backend=clipboard-paste status=starting bytes=97   ← never finishes
```

That save runs `wl-paste --no-newline --type text/plain;charset=utf-8`. But a
Wayland clipboard is not a buffer — it is a *promise* served by the client that
owns the selection. Typeless runs both ends: it owns the selection (through
XWayland) and blocks waiting for its own read to come back. The read never
resolves, the main process parks in `anon_pipe_read` on the child's pipe, and
the UI thread is gone with it. The child sits in `do_wait`; killing it does not
help, because spawned Electron children inherited the pipe's write end, so EOF
never arrives.

Measured on the hung machine:

```
$ wl-paste --no-newline --type text/plain;charset=utf-8   # hangs, killed at 5 s
$ wl-copy --clear
$ wl-paste --no-newline --type text/plain;charset=utf-8   # returns instantly
```

**Fix.** Give the clipboard a persistent owner so no read ever waits on a
client that is itself blocked:

```sh
wl-clip-persist --clipboard regular
```

`regular`, not the default `both`: Typeless pastes with `Ctrl+V`, i.e. the
regular clipboard. Persisting the primary selection too would route every
drag-select round-trip through the persister and keep resurrecting stale text
for middle-click paste, for no benefit.

Keep it running at login. On Omarchy, `~/.config/hypr/autostart.lua`:

```lua
o.launch_on_start("wl-clip-persist --clipboard regular")
```

The package makes `wl-clip-persist` a hard dependency and its `post_install`
errors out if no persister is running or configured, because without one the app
can freeze on the second dictation.

**Recovery if it already froze.** `wl-copy --clear` unsticks the clipboard, but
the app must be restarted — its main process is deadlocked, and it ignores
`SIGTERM` (use `kill -9`).

---

## Why the app is pinned to XWayland

`--ozone-platform=x11` in both the `.desktop` launcher and the autostart entry.
Upstream's overlay sets an X11 input hint on a Wayland surface, and Xlib aborts:

```
X Error of failed request: BadWindow ... X_ChangeProperty
```

Native Wayland crashes on startup here. Drop the flag from both `Exec=` lines if
upstream fixes it.

## What is deliberately *not* done

- **No binary patching.** Every fix lives outside `/opt/Typeless` (a preload
  shim, udev rules, desktop entries, compositor rules), so `pkgrel` bumps are
  the only maintenance when upstream re-releases.
- **Debian-only pieces skipped:** the AppArmor profile (AppArmor is not enabled
  on Arch by default) and the Fcitx5 addon `.so`, which upstream does not ship
  in this `.deb` at all. The IBus component XML is written but `ibus` is only an
  optdepend.

## Caveats

- **`uaccess` on every input device** is broader than it needs to be:
  `ENV{ID_INPUT}!=""` matches mice and touchpads too, and the helper could in
  principle open any of them. Narrowing to `ENV{ID_INPUT_KEYBOARD}=="1"` should
  work (the shim already hides pointer nodes) but is not verified here — the
  rule as shipped is the shape that was tested.
- **Voice IME path never succeeds.** The log always shows
  `backend=voice-ime status=failed stage=switch-engine target=typeless-voice`
  followed by the clipboard fallback. Harmless, just noisier than it needs to be.
- **Licence.** The `.deb`'s control file declares `MIT`, upstream publishes no
  source, and the bundle contains Chromium/Electron. This repo therefore does
  not redistribute the app at all — the PKGBUILD downloads it from
  `typeless-static.com` and pins its SHA-256 — and declares
  `LicenseRef-proprietary` rather than asserting a licence it cannot verify.
  The files written *here* are MIT (see [`LICENSE`](LICENSE)).

## Verified environment

Arch Linux, Hyprland 0.56.2 with Omarchy (Lua config), Wayland session,
fcitx5, monitor 3440×1440, Typeless 2.2.1, Logitech G304 + USB-HID keyboard.

## Files

```
PKGBUILD                  build from the upstream .deb
typeless.install          post_install: sandbox perms, group, udev reload,
                          clipboard-persister check
typeless-launcher.sh      /usr/bin/typeless — sets LD_PRELOAD, execs the app
typeless-shim.c           the LD_PRELOAD shim (fix 1)
hypr/typeless.lua         the two window rules (fixes 3 and 4)
```

Not affiliated with Typeless. No warranty; this reshapes an app that upstream
has not documented for Arch or Wayland.
