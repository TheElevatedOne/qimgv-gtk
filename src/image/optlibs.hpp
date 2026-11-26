// Optional codec libraries (libjxl, libavif, libheif, LibRaw), loaded at run time with dlopen().
//
// qimgv never links against these, so a missing library or a soname bump (libjxl.so.0.12 -> 0.13)
// only disables that format instead of stopping qimgv from starting. The unversioned "libfoo.so"
// is tried first, then the newest "libfoo.so.*" found in the usual library directories.
// Headers are only needed at build time; a library whose headers were missing is never loaded.
// All functions are thread safe; the libraries are loaded once, on first use.
#pragma once

#include <string>

#ifdef QIMGV_WITH_JXL
#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/resizable_parallel_runner.h>
#endif
#ifdef QIMGV_WITH_AVIF
#include <avif/avif.h>
#endif
#ifdef QIMGV_WITH_HEIF
#include <libheif/heif.h>
#endif
#ifdef QIMGV_WITH_RAW
#include <libraw/libraw.h>
#endif

#define QIMGV_FN(name) decltype(&::name) name = nullptr;

namespace qimgv {

#ifdef QIMGV_WITH_JXL
#define QIMGV_JXL_FUNCS(X) \
    X(JxlDecoderVersion) \
    X(JxlDecoderCreate) \
    X(JxlDecoderDestroy) \
    X(JxlDecoderSubscribeEvents) \
    X(JxlDecoderSetParallelRunner) \
    X(JxlDecoderSetInput) \
    X(JxlDecoderCloseInput) \
    X(JxlDecoderProcessInput) \
    X(JxlDecoderGetBasicInfo) \
    X(JxlDecoderSetPreferredColorProfile) \
    X(JxlDecoderGetFrameHeader) \
    X(JxlDecoderImageOutBufferSize) \
    X(JxlDecoderSetImageOutBuffer) \
    X(JxlColorEncodingSetToSRGB) \
    X(JxlEncoderCreate) \
    X(JxlEncoderDestroy) \
    X(JxlEncoderInitBasicInfo) \
    X(JxlEncoderSetBasicInfo) \
    X(JxlEncoderSetColorEncoding) \
    X(JxlEncoderFrameSettingsCreate) \
    X(JxlEncoderSetFrameLossless) \
    X(JxlEncoderSetFrameDistance) \
    X(JxlEncoderDistanceFromQuality) \
    X(JxlEncoderAddImageFrame) \
    X(JxlEncoderCloseInput) \
    X(JxlEncoderProcessOutput)
#define QIMGV_JXL_THREAD_FUNCS(X) \
    X(JxlResizableParallelRunner) \
    X(JxlResizableParallelRunnerCreate) \
    X(JxlResizableParallelRunnerDestroy) \
    X(JxlResizableParallelRunnerSetThreads) \
    X(JxlResizableParallelRunnerSuggestThreads)
struct JxlApi {
    QIMGV_JXL_FUNCS(QIMGV_FN)
    // libjxl_threads; all null when it is missing (decoding is then single threaded).
    QIMGV_JXL_THREAD_FUNCS(QIMGV_FN)
};
const JxlApi* jxlApi();
#endif

#ifdef QIMGV_WITH_AVIF
#define QIMGV_AVIF_FUNCS(X) \
    X(avifVersion) \
    X(avifResultToString) \
    X(avifDecoderCreate) \
    X(avifDecoderDestroy) \
    X(avifDecoderSetIOFile) \
    X(avifDecoderParse) \
    X(avifDecoderNextImage) \
    X(avifRGBImageSetDefaults) \
    X(avifRGBImageAllocatePixels) \
    X(avifRGBImageFreePixels) \
    X(avifImageYUVToRGB) \
    X(avifImageCreate) \
    X(avifImageDestroy) \
    X(avifImageRGBToYUV) \
    X(avifEncoderCreate) \
    X(avifEncoderDestroy) \
    X(avifEncoderWrite) \
    X(avifRWDataFree)
struct AvifApi {
    QIMGV_AVIF_FUNCS(QIMGV_FN)
};
const AvifApi* avifApi();
#endif

#ifdef QIMGV_WITH_HEIF
#define QIMGV_HEIF_FUNCS(X) \
    X(heif_init) \
    X(heif_context_alloc) \
    X(heif_context_free) \
    X(heif_context_read_from_file) \
    X(heif_context_get_primary_image_handle) \
    X(heif_image_handle_get_number_of_thumbnails) \
    X(heif_image_handle_get_list_of_thumbnail_IDs) \
    X(heif_image_handle_get_thumbnail) \
    X(heif_image_handle_get_width) \
    X(heif_image_handle_get_height) \
    X(heif_image_handle_release) \
    X(heif_image_handle_get_luma_bits_per_pixel) \
    X(heif_image_handle_has_alpha_channel) \
    X(heif_decoding_options_alloc) \
    X(heif_decoding_options_free) \
    X(heif_decode_image) \
    X(heif_image_get_width) \
    X(heif_image_get_height) \
    X(heif_image_get_plane_readonly) \
    X(heif_image_get_bits_per_pixel_range) \
    X(heif_image_release)
struct HeifApi {
    QIMGV_HEIF_FUNCS(QIMGV_FN)
};
const HeifApi* heifApi();
#endif

#ifdef QIMGV_WITH_RAW
#define QIMGV_RAW_FUNCS(X) \
    X(libraw_versionNumber) \
    X(libraw_strerror) \
    X(libraw_init) \
    X(libraw_close) \
    X(libraw_open_file) \
    X(libraw_unpack) \
    X(libraw_unpack_thumb) \
    X(libraw_dcraw_process) \
    X(libraw_dcraw_make_mem_image) \
    X(libraw_dcraw_make_mem_thumb) \
    X(libraw_dcraw_clear_mem) \
    X(libraw_set_dataerror_handler) \
    X(libraw_set_output_bps) \
    X(libraw_set_output_color)
struct RawApi {
    // True when the loaded LibRaw has the same major.minor version as the headers qimgv was built
    // with, so the libraw_data_t fields may be used. Otherwise only the functions are called.
    bool sameAbi = false;
    QIMGV_RAW_FUNCS(QIMGV_FN)
};
const RawApi* rawApi();
#endif

// Availability, for the format list. False when qimgv was built without the headers.
bool haveJxl();
bool haveAvif();
bool haveHeif();
bool haveRaw();

// One line per optional library: what was loaded from where, or why not (for --version).
std::string optionalLibrariesReport();

// Open a shared library by its base name ("libjxl"): "libjxl.so" first, then the newest
// "libjxl.so.*" in the library directories. Returns the dlopen() handle or nullptr.
void* openSharedLibrary(const char* base, std::string* loadedFrom = nullptr);

}  // namespace qimgv
