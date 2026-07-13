# Obstacle Type Icons

Store user-created obstacle category icons in these folders:

- `enabled/<button_name>.png`
- `disabled/<button_name>.png`
- `preview/<button_name>.png`

Use the same lowercase snake-case filename for both states. For example:

- `enabled/01_dirt_mountains.png`
- `disabled/01_dirt_mountains.png`

The two-digit prefix maps the icon to its one-based button number.

Icon requirements:

- 24 x 24 pixels;
- 32-bit RGBA PNG;
- transparent background with straight alpha;
- no button border, hover fill, or selected-state background.

Preview requirements:

- 160 x 160 pixels;
- 32-bit RGBA PNG;
- transparent background;
- full color only; no disabled preview is needed.

The plugin draws previews in a 164 x 164 white image area: 160 x 160 pixels
directly adjoining a two-pixel border. A fixed 23-pixel caption area appears
below it on `#f0f0f0`, separated by a two-pixel divider. Preview alpha is
preserved, including semi-transparent native-style shadows.

HotA reference images remain under `scratch/hota_me_resources` and must not be
copied here or committed.

Regenerate the embedded icon table after adding or changing icons:

```powershell
python tools\generate_obstacle_icons.py
```
