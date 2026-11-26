#include "core/previewfilter.h"

#include <vector>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace wc {

namespace {

std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.5f", v);
    std::string s(buf);
    while (s.size() > 1 && s.back() == '0' && s.find('.') != std::string::npos) s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

std::string quotePath(const std::string& p) {
    std::string out = "'";
    for (char c : p) {
        if (c == '\'') out += "\\'";
        else out += c;
    }
    out += "'";
    return out;
}

// Linear interpolation of a keyframed value at time t.
double evalKeyframes(const std::vector<std::pair<double, double>>& pts, double t) {
    if (pts.empty()) return 0.0;
    if (pts.size() == 1 || t <= pts.front().first) return pts.front().second;
    if (t >= pts.back().first) return pts.back().second;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        if (t >= pts[i].first && t <= pts[i + 1].first) {
            const double span = pts[i + 1].first - pts[i].first;
            const double f = span > 0 ? (t - pts[i].first) / span : 0.0;
            return pts[i].second + (pts[i + 1].second - pts[i].second) * f;
        }
    }
    return pts.back().second;
}

std::string escapeDrawtext(const std::string& in) {
    std::string out;
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            case ':': out += "\\:"; break;
            case '%': out += "\\%"; break;
            default: out += c; break;
        }
    }
    return out;
}

} // namespace

std::string buildPreviewVideoFilter(const FilterModel& f, const VideoInfo& info, double t) {
    (void)info;   // reserved for future geometry-aware preview decisions
    std::vector<std::string> chain;

    if (f.denoise) {
        static const char* presets[] = {"hqdn3d=1.5:1.5:6:6", "hqdn3d=2:2:8:8", "hqdn3d=3:3:10:10"};
        int idx = f.denoiseStrength;
        if (idx < 0) idx = 0;
        if (idx > 2) idx = 2;
        chain.push_back(presets[idx]);
    }

    if (f.crop.enabled && f.crop.w > 0 && f.crop.h > 0) {
        chain.push_back("crop=" + std::to_string(f.crop.w) + ":" + std::to_string(f.crop.h) +
                        ":" + std::to_string(f.crop.x) + ":" + std::to_string(f.crop.y));
    } else if (f.dynamicCrop.enabled && f.dynamicCrop.keyframes.size() >= 2) {
        std::vector<std::pair<double, double>> xs, ys;
        for (const auto& k : f.dynamicCrop.keyframes) {
            xs.emplace_back(k.time, static_cast<double>(k.x));
            ys.emplace_back(k.time, static_cast<double>(k.y));
        }
        const int w = f.dynamicCrop.w > 0 ? f.dynamicCrop.w : f.dynamicCrop.keyframes.front().w;
        const int h = f.dynamicCrop.h > 0 ? f.dynamicCrop.h : f.dynamicCrop.keyframes.front().h;
        chain.push_back("crop=" + std::to_string(w) + ":" + std::to_string(h) +
                        ":x=" + std::to_string(static_cast<int>(std::lround(evalKeyframes(xs, t)))) +
                        ":y=" + std::to_string(static_cast<int>(std::lround(evalKeyframes(ys, t)))));
    }

    if (f.resizeEnabled && f.resizeW > 0 && f.resizeH > 0) {
        std::string algo = f.resizeAlgo.empty() ? "bicubic" : f.resizeAlgo;
        chain.push_back("scale=" + std::to_string(f.resizeW) + ":" + std::to_string(f.resizeH) +
                        ":flags=" + algo);
    }

    if (f.rotateDegrees % 360 != 0) {
        int deg = ((f.rotateDegrees % 360) + 360) % 360;
        if (deg == 90) chain.emplace_back("transpose=1");
        else if (deg == 270) chain.emplace_back("transpose=2");
        else if (deg == 180) { chain.emplace_back("transpose=1"); chain.emplace_back("transpose=1"); }
    }

    if (f.eq.enabled) {
        chain.push_back("eq=gamma=" + num(f.eq.gamma) + ":saturation=" + num(f.eq.saturation) +
                        ":contrast=" + num(f.eq.contrast));
    }

    if (f.caption.enabled && !f.caption.text.empty()) {
        const bool inRange = (f.caption.start <= 0.0 && f.caption.end <= 0.0) ||
                             (t >= f.caption.start && t <= f.caption.end);
        if (inRange) {
            std::string d = "drawtext=text='" + escapeDrawtext(f.caption.text) + "'";
            d += ":fontcolor=" + f.caption.color;
            d += ":fontsize=" + std::to_string(f.caption.fontSize);
            d += ":borderw=2:bordercolor=" + f.caption.borderColor;
            d += ":x=" + std::to_string(f.caption.x) + ":y=" + std::to_string(f.caption.y);
            chain.push_back(d);
        }
    }

    std::string main;
    for (size_t i = 0; i < chain.size(); ++i) {
        if (i) main += ",";
        main += chain[i];
    }

    const bool overlayOn = f.overlay.enabled && !f.overlay.imagePath.empty() &&
                           ((f.overlay.start <= 0.0 && f.overlay.end <= 0.0) ||
                            (t >= f.overlay.start && t <= f.overlay.end));
    if (!overlayOn)
        return main;

    // Overlay needs a movie source: build a small graph around the main chain.
    std::string graph = "movie=" + quotePath(f.overlay.imagePath) + "[wm];[in]";
    graph += main.empty() ? std::string("null") : main;
    graph += "[base];[base][wm]overlay=" + std::to_string(f.overlay.x) + ":" +
             std::to_string(f.overlay.y) + ":shortest=1";
    return graph;
}

} // namespace wc
