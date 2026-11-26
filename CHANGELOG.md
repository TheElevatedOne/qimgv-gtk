# Changelog

## 0.1.1

- Fixed: qimgv did not start after a libjxl update (`libjxl.so.0.12: cannot open shared object file`).
- Changed: libjxl, libavif, libheif and LibRaw are optional and loaded at run time by their unversioned
  `.so` name, so a missing library or a new soname only turns off that format. `.jxl`, `.avif` and `.heic`
  fall back to gdk-pixbuf loaders when the library is missing.
- Changed: exiv2 moved into an optional plugin, `qimgv-exiv2.so` (installed to `/usr/lib/qimgv`). Without
  it, EXIF orientation of JPEG and WebP files is still applied.
- Added: `qimgv --version` lists the optional libraries that were loaded.

## 0.1.0

First release of the GTK 3 rewrite of qimgv.

- Added: C++17 / GTK 3 viewer with qimgv's layout and default shortcuts: thumbnail panel, folder view
  with bookmarks and a filesystem tree, in-window context menu, info bar, fullscreen info and controls.
- Added: native decoders for JPEG XL, AVIF, HEIF/HEIC, APNG and camera RAW (LibRaw), next to JPEG, PNG,
  GIF and WebP. Other formats are read through gdk-pixbuf.
- Added: animated GIF, APNG, WebP, JPEG XL and AVIF.
- Added: saving to JPEG XL and AVIF besides JPEG, PNG and WebP. EXIF metadata is copied and the
  orientation reset.
- Added: colours follow the GTK theme; qimgv's icons are recoloured to the theme's text colour.
- Changed: settings are stored in `~/.config/qimgv/qimgv-gtk.conf`.
- Removed: video playback (libmpv) and the `player_mpv` plugin. Video files are skipped when browsing.
- Removed: Windows and macOS support, Qt theme editor, translations.
