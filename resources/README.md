# Иконка

Сборка ожидает `resources/icon.ico` -- многослойный `.ico` (как
минимум 16x16, 32x32, 48x48, 256x256), встраиваемый в exe и в
трей через `resources/app.rc`.

SVG сам по себе Win32-ресурсом быть не может -- нужен `.ico`.
Если у вас уже есть `icon.svg`, положите его в `resources/icon.svg`
(для истории/переиспользования) и сконвертируйте одним из способов:

## Через ImageMagick (просто, кросс-платформенно)

```bash
# в WSL/Linux: sudo apt install imagemagick
magick resources/icon.svg -define icon:auto-resize=256,48,32,16 resources/icon.ico
```

## Через Inkscape + ImageMagick (если ImageMagick без librsvg)

```bash
for sz in 16 32 48 256; do
  inkscape resources/icon.svg -w $sz -h $sz -o resources/icon_$sz.png
done
magick resources/icon_16.png resources/icon_32.png resources/icon_48.png resources/icon_256.png resources/icon.ico
rm resources/icon_*.png
```

## Онлайн (без установки чего-либо)

Любой конвертер вида "SVG to ICO" с опцией multi-size (например
convertio.co, icoconvert.com) -- главное чтобы в один `.ico` вошли
несколько размеров, иначе иконка будет мыльной в одних местах
интерфейса (трей/Alt+Tab/проводник используют разные размеры).

После того как `resources/icon.ico` появился -- `make clean && make`
подхватит его автоматически (см. `resources/app.rc` и `Makefile`).
Без файла сборка **осознанно падает** на этапе `windres` с понятной
ошибкой, а не тихо собирает exe без иконки.
