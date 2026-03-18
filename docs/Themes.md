# Themes

Themes control colors, roundness, alpha, optional per-theme audio, and optional image assets.

Theme file:

`sd:/3ds/FirmMux/themes/<name>/theme.yaml`

## Common keys

- `name`
- `panel_alpha` (0–100; opacity of UI panels)
- `row_padding`
- `tab_padding`

Notes:

- FirmMux uses a fixed UI font for stability.
- FirmMux uses a fixed line spacing of `26` for stability.
- FirmMux uses a fixed list item height of `20` and a fixed status bar height of `16` for stability.
- FirmMux backgrounds now render at full opacity for stability.
- Older `theme.yaml` files that still contain `font_scale_top`, `font_scale_bottom`, or `font_path` are safe; those keys are ignored.
- Older `theme.yaml` files that still contain `line_spacing` are also safe; that key is ignored.
- Older `theme.yaml` files that still contain `list_item_h` or `status_bar_h` are also safe; those keys are ignored.

## Accent

- `accent`
  - If set, it tints selection highlights (tabs, lists, options).

## Roundness

- `radius_global`
- `radius_tabs`
- `radius_list`
- `radius_options`
- `radius_panels`
- `radius_preview`
- `radius_status`
- `radius_picker`

Per-element values override `radius_global`.

## Theme audio (optional)

- `ui_sounds_dir`
  - Example: `sd:/3ds/FirmMux/themes/mytheme/sounds/`
  - If not set, defaults to `sd:/3ds/FirmMux/ui sounds/`
- Sound filenames (theme folder):
  - `tap_01.wav` (or `tap.wav`)
  - `select.wav`
  - `toggle_off.wav`
  - `swipe_01.wav` (or `swipe.wav`)
  - `toggle_on.wav`
  - `caution.wav`
- `bgm_path`
  - Example: `sd:/3ds/FirmMux/themes/mytheme/bgm.wav`
  - If not set, defaults to `sd:/3ds/FirmMux/bgm/bgm.wav`

## Theme wallpapers (optional)

- `top_image`: `topbg.png` (drawn full-screen on top)
- `bottom_image`: `botbg.png` (drawn full-screen on bottom)

If a background is selected in Options, it overrides the theme wallpaper for that screen.
Older saved background visibility values are safe and ignored.

## Theme image assets (optional)

Supported image keys:

- `status_strip`
- `sprite_icon`
- `list_item_image`
- `list_sel_image`
- `tab_item_image`
- `tab_sel_image`
- `option_item_image`
- `option_sel_image`
- `preview_frame`
- `help_strip`

Notes:

- These assets are optional.
- If an asset is missing, FirmMux falls back to the normal color-based UI for that element.
- Wallpapers and UI assets use the same safe texture upload path used for stable cover preview handling.

## Theme options debug menu

Options → Theme options…

- Adjusts panel alpha and per-element alpha live.
- Also adjusts row padding and tab padding.
- Debug menu overlays show bounds for panels/tabs/lists/preview/help/status.

Current alpha controls:

- `Preview alpha`
- `Tab alpha`
- `List alpha`
- `Overlay alpha`
- `Toast alpha`

## Colors

All colors are hex RGB or ARGB:

- `top_bg`
- `bottom_bg`
- `panel_left`
- `panel_right`
- `preview_bg`
- `text_primary`
- `text_secondary`
- `text_muted`
- `tab_bg`
- `tab_sel`
- `tab_text`
- `list_bg`
- `list_sel`
- `list_text`
- `option_bg`
- `option_sel`
- `option_text`
- `option_header`
- `overlay_bg`
- `help_bg`
- `help_line`
- `help_text`
- `status_bg`
- `status_text`
- `status_icon`
- `status_dim`
- `status_bolt`
- `toast_bg`
- `toast_text`

## Included themes

- neon_tokyo
- crystal_harbor
- midnight_ramen
- velvet_gruv
- orbit_dreams
- golden_drizzle

Repo-side example/test theme:

- `themes/test_assets`
