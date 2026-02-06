# Themes

Themes control colors, spacing, roundness, font scale, and optional per-theme audio.

Theme file:

`sd:/3ds/FirmMux/themes/<name>/theme.yaml`

## Common keys

- `name`
- `list_item_h`
- `line_spacing`
- `status_bar_h`
- `font_scale_top`
- `font_scale_bottom`
- `panel_alpha` (0–100; opacity of UI panels)
- `row_padding`
- `tab_padding`

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
  - `tap.wav`
  - `select.wav`
  - `toggle_off.wav`
  - `swipe.wav`
  - `toggle_on.wav`
  - `caution.wav`
- `bgm_path`
  - Example: `sd:/3ds/FirmMux/themes/mytheme/bgm.wav`
  - If not set, defaults to `sd:/3ds/FirmMux/bgm/bgm.wav`

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
