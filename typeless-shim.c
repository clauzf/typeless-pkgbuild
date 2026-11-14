/*
 * typeless-shim: runtime fixes for Typeless on Linux, loaded via LD_PRELOAD.
 *
 * Why this exists instead of patching the shipped binary:
 *
 *  1. Typeless enumerates /dev/input/event* and EVIOCGRABs every device that
 *     reports EV_KEY plus KEY_A/KEY_ENTER. Logitech receivers report keyboard
 *     usages on the mouse node, so the mouse qualifies, gets grabbed, and the
 *     compositor stops receiving pointer motion -- the cursor freezes.
 *     This shim opens each candidate first and makes pointer devices (those
 *     with EV_REL or EV_ABS axes: mice, touchpads, tablets) look absent, so
 *     Typeless never grabs or replays them.
 *
 *  2. Typeless advertises uinput virtual keyboards with the full keycode range
 *     (0x0..0x2ff), which includes BTN_MOUSE. libinput then classifies them as
 *     pointers as well as keyboards, and Hyprland keeps phantom pointers with
 *     no cursor image. Dropping UI_SET_KEYBIT for codes >= 0x100 keeps those
 *     devices keyboard-only; pasting is unaffected because Ctrl+V uses KEY_V.
 *
 * Everything is intercepted at the libc boundary, so an upstream Typeless
 * update keeps working without re-patching any binary.
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* uinput takes the bit number as the raw ioctl argument. */
#define UI_SET_KEYBIT_CODE 0x40045565UL

/* Codes below this are KEY_*; 0x100 and up are BTN_* (mouse, gamepad...). */
#define KEY_CODE_LIMIT 0x100

typedef int (*open_fn)(const char *, int, ...);
typedef int (*openat_fn)(int, const char *, int, ...);
typedef int (*ioctl_fn)(int, unsigned long, void *);

static open_fn real_open;
static open_fn real_open64;
static openat_fn real_openat;
static ioctl_fn real_ioctl;

static void resolve_symbols(void) {
  if (real_open == NULL) {
    real_open = (open_fn)dlsym(RTLD_NEXT, "open");
  }
  if (real_open64 == NULL) {
    real_open64 = (open_fn)dlsym(RTLD_NEXT, "open64");
  }
  if (real_openat == NULL) {
    real_openat = (openat_fn)dlsym(RTLD_NEXT, "openat");
  }
  if (real_ioctl == NULL) {
    real_ioctl = (ioctl_fn)dlsym(RTLD_NEXT, "ioctl");
  }
}

/* True when the descriptor reports relative or absolute axes. */
static bool has_pointer_axes(int fd) {
  unsigned long bits = 0;

  if (real_ioctl == NULL) {
    return false;
  }
  if (real_ioctl(fd, EVIOCGBIT(EV_REL, sizeof(bits)), &bits) >= 0 && bits != 0) {
    return true;
  }
  bits = 0;
  if (real_ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bits)), &bits) >= 0 && bits != 0) {
    return true;
  }
  return false;
}

/* Hide pointer event nodes the way a missing device would look. */
static int filter_opened(int fd, const char *path) {
  if (fd < 0 || path == NULL) {
    return fd;
  }
  if (strncmp(path, "/dev/input/event", strlen("/dev/input/event")) != 0) {
    return fd;
  }
  if (!has_pointer_axes(fd)) {
    return fd;
  }
  close(fd);
  errno = ENOENT;
  return -1;
}

int open(const char *path, int flags, ...) {
  va_list args;
  mode_t mode = 0;

  resolve_symbols();
  va_start(args, flags);
  mode = (mode_t)va_arg(args, int);
  va_end(args);

  return filter_opened(real_open(path, flags, mode), path);
}

int open64(const char *path, int flags, ...) {
  va_list args;
  mode_t mode = 0;

  resolve_symbols();
  va_start(args, flags);
  mode = (mode_t)va_arg(args, int);
  va_end(args);

  return filter_opened(real_open64(path, flags, mode), path);
}

int openat(int dirfd, const char *path, int flags, ...) {
  va_list args;
  mode_t mode = 0;

  resolve_symbols();
  va_start(args, flags);
  mode = (mode_t)va_arg(args, int);
  va_end(args);

  return filter_opened(real_openat(dirfd, path, flags, mode), path);
}

int ioctl(int fd, unsigned long request, ...) {
  va_list args;
  void *argument;

  resolve_symbols();
  va_start(args, request);
  argument = va_arg(args, void *);
  va_end(args);

  if (request == UI_SET_KEYBIT_CODE) {
    unsigned long code = (unsigned long)(uintptr_t)argument;
    if (code >= KEY_CODE_LIMIT) {
      /* Accept the call but never advertise pointer buttons. */
      return 0;
    }
  }
  return real_ioctl(fd, request, argument);
}
