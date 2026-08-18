# Icon

The build expects `resources/icon.ico` -- a multi-size `.ico` (at
least 16x16, 32x32, 48x48, 256x256), embedded into the exe and the
tray icon via `resources/app.rc`.

An SVG can't be a Win32 resource by itself -- you need a `.ico`.
If you already have `icon.svg`, drop it at `resources/icon.svg` (for
reference/reuse) and convert it with one of:

## ImageMagick (simplest, cross-platform)

```bash
# on WSL/Linux: sudo apt install imagemagick
magick resources/icon.svg -define icon:auto-resize=256,48,32,16 resources/icon.ico
```

## Inkscape + ImageMagick (if ImageMagick lacks librsvg)

```bash
for sz in 16 32 48 256; do
  inkscape resources/icon.svg -w $sz -h $sz -o resources/icon_$sz.png
done
magick resources/icon_16.png resources/icon_32.png resources/icon_48.png resources/icon_256.png resources/icon.ico
rm resources/icon_*.png
```

## Online (no local install)

Any "SVG to ICO" converter with a multi-size option (e.g.
convertio.co, icoconvert.com) -- multiple sizes in one `.ico` matter,
otherwise the icon looks blurry in some places (tray/Alt+Tab/Explorer
use different sizes).

Once `resources/icon.ico` exists, `make clean && make` picks it up
automatically (see `resources/app.rc` and `Makefile`). Without the
file the build **intentionally fails** at the `windres` step with a
clear error, rather than silently shipping an exe with no icon.
