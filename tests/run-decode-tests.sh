#!/bin/sh
# Generate one sample per supported format and decode them with qimgv-decode-test.
# Needs ImageMagick (magick), cjxl, avifenc, heif-enc and ffmpeg.
# Usage: tests/run-decode-tests.sh <build-dir>
set -eu
build=$(cd "$1" && pwd)
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
cd "$dir"

magick -size 300x200 gradient:red-blue base.png
magick base.png -quality 90 base.jpg
magick base.png base.webp
magick base.png base.bmp
magick base.png base.tiff
magick -size 64x64 xc:none -fill green -draw "circle 32,32 32,4" alpha.png
cjxl -q 90 base.png base.jxl >/dev/null 2>&1
cjxl base.jpg recompressed.jxl >/dev/null 2>&1
avifenc base.png base.avif >/dev/null 2>&1
heif-enc base.png -o base.heic >/dev/null 2>&1
ffmpeg -loglevel error -f lavfi -i "testsrc=size=120x90:rate=10:duration=1" -plays 0 -f apng anim.apng
ffmpeg -loglevel error -f lavfi -i "testsrc=size=120x90:rate=10:duration=1" anim.gif
ffmpeg -loglevel error -f lavfi -i "testsrc=size=120x90:rate=10:duration=1" -loop 0 anim.webp
cjxl anim.gif anim.jxl >/dev/null 2>&1
printf '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="80"><rect width="100" height="80" fill="orange"/></svg>' > a.svg

"$build/qimgv-decode-test" ./*.png ./*.jpg ./*.webp ./*.bmp ./*.tiff ./*.jxl ./*.avif ./*.heic ./*.apng ./*.gif ./*.svg

# Animations must keep their frames.
for f in anim.apng anim.gif anim.webp anim.jxl; do
  "$build/qimgv-decode-test" "$f" | grep -q "frames=10" || { echo "FAIL  $f: expected 10 frames"; exit 1; }
done

# Videos are rejected.
touch clip.mp4
if "$build/qimgv-decode-test" clip.mp4 >/dev/null; then
  echo "FAIL  clip.mp4 was accepted"
  exit 1
fi
echo "All decode tests passed."
