# Maintainer: unofficial Arch packaging (see README for the runtime patches)
pkgname=typeless
pkgver=2.2.1
pkgrel=9
pkgdesc='Speech-to-text dictation that turns spoken words into polished text'
arch=('x86_64')
url='https://www.typeless.com/'
# Upstream's .deb metadata says MIT, but no source is published and the bundle
# carries Chromium/Electron, so the redistributable terms cannot be verified.
# Do not assert a permissive licence on someone else's binary.
license=('LicenseRef-proprietary')
provides=('typeless-bin')
conflicts=('typeless-bin')
depends=(
  gtk3
  libnotify
  nss
  libxss
  libxtst
  libx11
  xdg-utils
  at-spi2-core
  libsecret
  util-linux
  acl
  procps-ng
  libpulse
  wl-clipboard
  mesa
  libglvnd
  # Hard dependency: Typeless pastes by reading the clipboard back, and without
  # a persisting owner that read can block forever (see typeless.install).
  wl-clip-persist
  # Electron/Chromium shared libraries are linked dynamically and are not all
  # pulled in by gtk3; without these the app fails to start on a lean install.
  alsa-lib
  cairo
  dbus
  expat
  gcc-libs
  glib2
  glibc
  libcups
  libdrm
  libxcb
  libxcomposite
  libxdamage
  libxext
  libxfixes
  libxkbcommon
  libxrandr
  nspr
  pango
  systemd-libs
)
optdepends=(
  'libibus: IBus voice input method engine'
  'ibus: register the Typeless engine with IBus'
  'fcitx5: Fcitx5 voice input method addon'
  'libappindicator: legacy tray indicator support'
)
options=(!strip !debug !lto)
install=typeless.install
# /etc file, so upgrades leave a .pacnew instead of silently replacing edits
backup=('etc/xdg/autostart/typeless.desktop')
source=(
  "https://typeless-static.com/desktop-release/Typeless-${pkgver}-x64.deb"
  typeless-shim.c
  typeless-launcher.sh
)
sha256sums=(
  '5130b311a58e8e3bf55a4e1410e0fc7f5d3c01a15b9c3a2c1619b224f14d76bc'
  'SKIP'
  'SKIP'
)

build() {
  # Runtime compatibility shim, loaded through LD_PRELOAD by the launcher.
  # Kept outside the app binary so upstream updates keep working.
  gcc -O2 -fPIC -shared -Wall -Wextra -Werror \
    -o "${srcdir}/typeless-shim.so" "${srcdir}/typeless-shim.c" -ldl
}

package() {
  bsdtar -xf "${srcdir}/Typeless-${pkgver}-x64.deb" data.tar.xz
  bsdtar -xf data.tar.xz -C "${pkgdir}"

  # electron-builder does not ship a /usr/bin wrapper; upstream's deb postinst
  # only symlinks. Ours loads the shim first.
  install -Dm755 "${srcdir}/typeless-launcher.sh" "${pkgdir}/usr/bin/typeless"
  install -Dm644 "${srcdir}/typeless-shim.so" "${pkgdir}/usr/lib/typeless/typeless-shim.so"

  # Chromium's setuid sandbox helper must be root:root and 4755 when it exists
  chmod 4755 "${pkgdir}/opt/Typeless/chrome-sandbox"

  # The keyboard helper reads raw /dev/input/event* nodes to detect the global
  # dictation hotkey. Upstream expects input-group membership plus an `sg input`
  # self-relaunch, but Arch's shadow package does not ship /usr/bin/sg, so the
  # relaunch never happens and every hotkey is missed. Give the active seat user
  # ACLs instead (same mechanism /dev/uinput already uses), which works without
  # a re-login. The rule must sort before 73-seat-late.rules, which is what runs
  # the uaccess builtin; a later number only adds a tag that nothing acts on.
  install -Dm644 /dev/stdin "${pkgdir}/usr/lib/udev/rules.d/72-typeless-input-uaccess.rules" <<'EOF'
# Typeless dictation hotkey needs read access to /dev/input/event*
SUBSYSTEM=="input", ENV{ID_INPUT}!="", TAG+="uaccess"
EOF

  # uinput access for the keyboard helper (mirrors the deb postinst)
  install -Dm644 /dev/stdin "${pkgdir}/usr/lib/udev/rules.d/70-uinput-uaccess.rules" <<'EOF'
ACTION!="remove", KERNEL=="uinput", SUBSYSTEM=="misc", MODE="0660", GROUP="input", OPTIONS+="static_node=uinput", TAG+="uaccess"
EOF
  install -Dm644 /dev/stdin "${pkgdir}/usr/lib/modules-load.d/typeless-uinput.conf" <<'EOF'
uinput
EOF

  # Upstream builds break under native Wayland: the overlay's X11 InputHint call
  # gets a Wayland surface handle and Xlib aborts with BadWindow (X_ChangeProperty).
  # Pin the app to XWayland, which works.
  install -Dm644 /dev/stdin "${pkgdir}/usr/share/applications/typeless.desktop" <<'EOF'
[Desktop Entry]
Name=Typeless
Exec=/usr/bin/typeless --ozone-platform=x11 %U
Terminal=false
Type=Application
Icon=typeless
StartupWMClass=Typeless
Comment=Speak naturally, and Typeless will turn your words into polished messages, emails, and documents that read like you carefully typed them.
MimeType=x-scheme-handler/typeless;
Categories=Utility;Office;
EOF

  # silent autostart at graphical login (deb postinst writes the same file)
  install -Dm644 /dev/stdin "${pkgdir}/etc/xdg/autostart/typeless.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Typeless
Comment=Typeless
Exec=env TYPELESS_SKIP_SECRET_STORE=1 /usr/bin/typeless --ozone-platform=x11 --system-startup-silent-launch --password-store=basic
Hidden=false
NoDisplay=true
X-GNOME-Autostart-enabled=true
X-GNOME-AutoRestart=false
Terminal=false
StartupNotify=false
EOF
}
