// C interface of the optional metadata plugin (qimgv-exiv2.so, src/plugins/exiv2).
// exiv2 has a C++ API and bumps its soname, so it lives in a plugin that qimgv dlopen()s: when exiv2 is
// missing or was updated past the version the plugin was built for, qimgv still starts and only loses
// the EXIF panel and metadata copying (orientation is then read by metadata.cpp itself).
#pragma once

#define QIMGV_METADATA_PLUGIN_API 1

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*qimgv_metadata_add_fn)(void* user, const char* label, const char* value);

// Returns QIMGV_METADATA_PLUGIN_API.
int qimgv_metadata_api_version(void);
// Short library description, e.g. "exiv2 0.28.7".
const char* qimgv_metadata_library(void);
void qimgv_metadata_init(void);
// EXIF orientation 1..8, 1 when missing.
int qimgv_metadata_orientation(const char* path);
// Calls `add` for each useful EXIF tag.
void qimgv_metadata_exif_summary(const char* path, qimgv_metadata_add_fn add, void* user);
void qimgv_metadata_copy(const char* src, const char* dst);

#ifdef __cplusplus
}
#endif
