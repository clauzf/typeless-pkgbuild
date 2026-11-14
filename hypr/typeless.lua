-- Typeless window rules for Hyprland (Omarchy Lua config).
--
-- Typeless renders its overlays as transparent, frameless Electron windows and
-- paints the visible pill/panel inside them. Hyprland draws a border around the
-- whole window box, so the transparent canvas shows up as an empty outlined
-- rectangle hovering over the desktop.
--
-- There are two such windows, with different identities:
--
--   Typeless-floating-bar   'Status'      750x500   dictation bar
--   Typeless                'AI result'   800x709   ask-anything panel
--
-- The second one reuses the main application's class, so it has to be matched
-- by title as well, otherwise it keeps a border.

-- Dictation bar: frameless transparent 750x500 canvas.
o.window(
  { class = "^Typeless-floating-bar$" },
  { border_size = 0, rounding = 0, no_shadow = true, no_blur = true, no_dim = true, no_anim = true }
)

-- "Ask anything" panel (Right Alt + Space): same class as the main window.
o.window(
  { class = "^Typeless$", title = "^AI result$" },
  { border_size = 0, rounding = 0, no_shadow = true, no_blur = true, no_dim = true, no_anim = true }
)
