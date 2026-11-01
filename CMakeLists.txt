cmake_minimum_required(VERSION 3.16)
project(qimgv VERSION 0.1.0
    DESCRIPTION "Fast and easy to use GTK image viewer"
    HOMEPAGE_URL "https://github.com/TheElevatedOne/qimgv-gtk"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(QIMGV_BUILD_TESTS "Build the decoder test tool" ON)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0>=3.24)
pkg_check_modules(IMGLIBS REQUIRED IMPORTED_TARGET
  libturbojpeg>=3.0
  libpng>=1.6
  libwebp
  libwebpdemux
  libjxl>=0.9
  libjxl_threads
  libavif>=1.0
  libheif>=1.16
  libraw_r>=0.21
  exiv2>=0.28)
find_library(GIF_LIBRARY NAMES gif REQUIRED)
find_path(GIF_INCLUDE_DIR gif_lib.h REQUIRED)
find_package(ZLIB REQUIRED)

# Icons and other runtime data are compiled into the binary as a GResource bundle.
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
set(GRESOURCE_XML ${CMAKE_CURRENT_BINARY_DIR}/qimgv.gresource.xml)
file(GLOB_RECURSE QIMGV_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_CURRENT_SOURCE_DIR}/data/icons/*.png)
list(SORT QIMGV_ICONS)
set(_res_files "")
foreach(icon ${QIMGV_ICONS})
  string(APPEND _res_files "    <file>${icon}</file>\n")
endforeach()
file(WRITE ${GRESOURCE_XML}.in
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gresources>\n  <gresource prefix=\"/qimgv\">\n${_res_files}    <file alias=\"logo.png\">hicolor/128x128/apps/qimgv.png</file>\n  </gresource>\n</gresources>\n")
configure_file(${GRESOURCE_XML}.in ${GRESOURCE_XML} COPYONLY)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources.c
  COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/data
          --target=${CMAKE_CURRENT_BINARY_DIR}/resources.c ${GRESOURCE_XML}
  DEPENDS ${GRESOURCE_XML} ${CMAKE_CURRENT_SOURCE_DIR}/data/hicolor/128x128/apps/qimgv.png
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data)

configure_file(src/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/config.hpp)

# ---- image decoding / encoding (no GUI) -----------------------------------
add_library(qimgvimage STATIC
  src/image/image.cpp
  src/image/formats.cpp
  src/image/decoder.cpp
  src/image/dec_jpeg.cpp
  src/image/dec_png.cpp
  src/image/dec_gif.cpp
  src/image/dec_webp.cpp
  src/image/dec_jxl.cpp
  src/image/dec_avif.cpp
  src/image/dec_heif.cpp
  src/image/dec_raw.cpp
  src/image/dec_pixbuf.cpp
  src/image/encoder.cpp
  src/image/metadata.cpp
  src/image/thumbnail.cpp)
target_include_directories(qimgvimage PUBLIC src ${CMAKE_CURRENT_BINARY_DIR} PRIVATE ${GIF_INCLUDE_DIR})
target_compile_options(qimgvimage PRIVATE -Wall -Wextra)
target_link_libraries(qimgvimage PUBLIC PkgConfig::GTK3 PRIVATE PkgConfig::IMGLIBS ${GIF_LIBRARY} ZLIB::ZLIB Threads::Threads)

# ---- application ----------------------------------------------------------
add_executable(qimgv
  src/main.cpp
  src/settings.cpp
  src/actions.cpp
  src/core/directorymodel.cpp
  src/core/loader.cpp
  src/core/thumbnailer.cpp
  src/core/fileops.cpp
  src/core/system.cpp
  src/gui/style.cpp
  src/gui/icons.cpp
  src/gui/viewer.cpp
  src/gui/thumbgrid.cpp
  src/gui/mainpanel.cpp
  src/gui/folderview.cpp
  src/gui/overlays.cpp
  src/gui/contextmenu.cpp
  src/gui/croppanel.cpp
  src/gui/dialogs.cpp
  src/gui/settingsdialog.cpp
  src/gui/mainwindow.cpp
  src/gui/mainwindow_actions.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/resources.c)
target_compile_options(qimgv PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
target_link_libraries(qimgv PRIVATE qimgvimage PkgConfig::GTK3 Threads::Threads)

install(TARGETS qimgv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/qimgv.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/qimgv.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(DIRECTORY data/hicolor DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)

if(QIMGV_BUILD_TESTS)
  add_executable(qimgv-decode-test tests/decode_test.cpp)
  target_link_libraries(qimgv-decode-test PRIVATE qimgvimage)
endif()
