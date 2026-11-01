#include "image/metadata.hpp"

#include <exiv2/exiv2.hpp>

#include <cmath>
#include <cstdio>
#include <mutex>

namespace qimgv {

void initMetadata() {
    static std::once_flag once;
    std::call_once(once, [] {
        Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
        Exiv2::LogMsg::setHandler(nullptr);
        Exiv2::XmpParser::initialize();
    });
}

static Exiv2::Image::UniquePtr openImage(const std::string& path) {
    initMetadata();
    auto img = Exiv2::ImageFactory::open(path);
    img->readMetadata();
    return img;
}

int readOrientation(const std::string& path) {
    try {
        auto img = openImage(path);
        auto& exif = img->exifData();
        auto it = exif.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
        if(it != exif.end() && it->count() > 0) {
            long v = (long)it->toInt64();
            if(v >= 1 && v <= 8) return (int)v;
        }
    } catch(...) {
    }
    return 1;
}

static std::string tagValue(Exiv2::ExifData& exif, const char* key) {
    try {
        auto it = exif.findKey(Exiv2::ExifKey(key));
        if(it == exif.end()) return {};
        std::string s = it->print(&exif);
        while(!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
        return s;
    } catch(...) {
        return {};
    }
}

static double rationalToDouble(const Exiv2::Value& v, size_t i) {
    auto r = v.toRational(i);
    return r.second ? (double)r.first / r.second : 0.0;
}

static std::string gpsCoord(Exiv2::ExifData& exif, const char* key, const char* refKey) {
    try {
        auto it = exif.findKey(Exiv2::ExifKey(key));
        if(it == exif.end() || it->count() < 3) return {};
        double deg = rationalToDouble(it->value(), 0) + rationalToDouble(it->value(), 1) / 60.0 +
                     rationalToDouble(it->value(), 2) / 3600.0;
        std::string ref = tagValue(exif, refKey);
        if(!ref.empty() && (ref[0] == 'S' || ref[0] == 'W')) deg = -deg;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6f", deg);
        return buf;
    } catch(...) {
        return {};
    }
}

InfoList readExifSummary(const std::string& path) {
    InfoList out;
    try {
        auto img = openImage(path);
        auto& exif = img->exifData();
        if(exif.empty()) return out;
        auto add = [&](const char* label, const std::string& v) {
            if(!v.empty()) out.emplace_back(label, v);
        };
        add("Make", tagValue(exif, "Exif.Image.Make"));
        add("Model", tagValue(exif, "Exif.Image.Model"));
        add("Lens", tagValue(exif, "Exif.Photo.LensModel"));
        add("Date/time", tagValue(exif, "Exif.Photo.DateTimeOriginal"));
        add("Exposure time", tagValue(exif, "Exif.Photo.ExposureTime"));
        add("F-number", tagValue(exif, "Exif.Photo.FNumber"));
        add("ISO", tagValue(exif, "Exif.Photo.ISOSpeedRatings"));
        add("Focal length", tagValue(exif, "Exif.Photo.FocalLength"));
        add("Focal length (35mm)", tagValue(exif, "Exif.Photo.FocalLengthIn35mmFilm"));
        add("Exposure bias", tagValue(exif, "Exif.Photo.ExposureBiasValue"));
        add("Flash", tagValue(exif, "Exif.Photo.Flash"));
        add("White balance", tagValue(exif, "Exif.Photo.WhiteBalance"));
        add("Software", tagValue(exif, "Exif.Image.Software"));
        add("Artist", tagValue(exif, "Exif.Image.Artist"));
        add("Copyright", tagValue(exif, "Exif.Image.Copyright"));
        std::string lat = gpsCoord(exif, "Exif.GPSInfo.GPSLatitude", "Exif.GPSInfo.GPSLatitudeRef");
        std::string lon = gpsCoord(exif, "Exif.GPSInfo.GPSLongitude", "Exif.GPSInfo.GPSLongitudeRef");
        if(!lat.empty() && !lon.empty()) out.emplace_back("GPS", lat + ", " + lon);
    } catch(...) {
    }
    return out;
}

void copyMetadata(const std::string& src, const std::string& dst) {
    try {
        auto in = openImage(src);
        auto out = Exiv2::ImageFactory::open(dst);
        out->readMetadata();
        Exiv2::ExifData exif = in->exifData();
        auto it = exif.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
        if(it != exif.end()) it->setValue("1");
        // Drop the embedded thumbnail: it would show the unedited image.
        Exiv2::ExifThumb thumb(exif);
        thumb.erase();
        out->setExifData(exif);
        out->setXmpData(in->xmpData());
        out->setIptcData(in->iptcData());
        out->writeMetadata();
    } catch(...) {
    }
}

}  // namespace qimgv
