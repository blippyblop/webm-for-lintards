#include "ui/filterjson.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cctype>

// Serialisation is *minimal*: only filters that are actually active are
// written. Presence of an object implies the filter is enabled; an absent key
// means "no filter for that option". This keeps saved JSON short and readable.
namespace wc {

namespace {
QJsonObject trimObj(const TrimRange& t) {
    QJsonObject o;
    o["start"] = t.start;
    o["end"] = t.end;
    return o;
}

// --- untrusted-input sanitisers ---
std::string safePath(std::string p) {
    if (p.size() > 4096) return std::string();
    for (unsigned char c : p)
        if (c < 0x20 || c == 0x7f || c == '\'') return std::string();
    return p;
}
std::string safeColor(std::string c, const char* fallback) {
    if (c.empty() || c.size() > 32) return fallback;
    for (unsigned char ch : c)
        if (!(std::isalnum(ch) || ch == '#')) return fallback;
    return c;
}
std::string safeAlgo(std::string a) {
    if (a == "bicubic" || a == "bilinear" || a == "lanczos" || a == "neighbor") return a;
    return "bicubic";
}
} // namespace

QJsonObject filterToJson(const FilterModel& f) {
    QJsonObject o;

    if (f.trim)
        o["trim"] = trimObj(*f.trim);

    if (!f.multiTrim.empty()) {
        QJsonArray a;
        for (const auto& t : f.multiTrim) a.append(trimObj(t));
        o["multiTrim"] = a;
    }

    if (f.crop.enabled) {
        QJsonObject c;
        c["x"] = f.crop.x; c["y"] = f.crop.y; c["w"] = f.crop.w; c["h"] = f.crop.h;
        o["crop"] = c;
    }

    if (f.dynamicCrop.enabled && !f.dynamicCrop.keyframes.empty()) {
        QJsonObject d;
        d["w"] = f.dynamicCrop.w; d["h"] = f.dynamicCrop.h;
        QJsonArray a;
        for (const auto& k : f.dynamicCrop.keyframes) {
            QJsonObject ko;
            ko["time"] = k.time; ko["x"] = k.x; ko["y"] = k.y; ko["w"] = k.w; ko["h"] = k.h;
            a.append(ko);
        }
        d["keyframes"] = a;
        o["dynamicCrop"] = d;
    }

    if (f.resizeEnabled) {
        QJsonObject r;
        r["w"] = f.resizeW; r["h"] = f.resizeH;
        r["algo"] = QString::fromStdString(f.resizeAlgo);
        o["resize"] = r;
    }

    if (f.rotateDegrees % 360 != 0)
        o["rotateDegrees"] = f.rotateDegrees;
    if (f.rateFps)
        o["rateFps"] = *f.rateFps;

    if (f.dynamicRateEnabled && !f.dynamicRate.empty()) {
        QJsonObject d;
        QJsonArray a;
        for (const auto& k : f.dynamicRate) {
            QJsonObject ko; ko["time"] = k.time; ko["rate"] = k.rate; a.append(ko);
        }
        d["keyframes"] = a;
        if (!f.dynamicRateSmoothing) d["smoothing"] = false;          // default true
        if (f.dynamicRateSmoothingOverlap != 0.5) d["overlap"] = f.dynamicRateSmoothingOverlap;
        o["dynamicRate"] = d;
    }

    if (f.caption.enabled) {
        QJsonObject c;
        c["text"] = QString::fromStdString(f.caption.text);
        c["fontFamily"] = QString::fromStdString(f.caption.fontFamily);
        c["fontSize"] = f.caption.fontSize;
        c["x"] = f.caption.x; c["y"] = f.caption.y;
        c["start"] = f.caption.start; c["end"] = f.caption.end;
        c["color"] = QString::fromStdString(f.caption.color);
        c["borderColor"] = QString::fromStdString(f.caption.borderColor);
        o["caption"] = c;
    }

    if (f.subtitle.enabled) {
        QJsonObject s;
        s["streamIndex"] = f.subtitle.streamIndex;
        s["path"] = QString::fromStdString(f.subtitle.path);
        s["burnIn"] = f.subtitle.burnIn;
        o["subtitle"] = s;
    }

    if (f.overlay.enabled) {
        QJsonObject v;
        v["imagePath"] = QString::fromStdString(f.overlay.imagePath);
        v["x"] = f.overlay.x; v["y"] = f.overlay.y;
        v["start"] = f.overlay.start; v["end"] = f.overlay.end;
        o["overlay"] = v;
    }

    if (f.fade.enabled) {
        QJsonObject v; v["in"] = f.fade.inDuration; v["out"] = f.fade.outDuration; o["fade"] = v;
    }

    if (f.eq.enabled) {
        QJsonObject v;
        v["gamma"] = f.eq.gamma; v["saturation"] = f.eq.saturation; v["contrast"] = f.eq.contrast;
        o["eq"] = v;
    }

    if (f.denoise) {
        o["denoise"] = true;
        if (f.denoiseStrength != 0) o["denoiseStrength"] = f.denoiseStrength;
    }
    if (f.deinterlace) o["deinterlace"] = true;
    if (f.delayAudioMs != 0) o["delayAudioMs"] = f.delayAudioMs;
    if (f.reverse) o["reverse"] = true;
    if (f.loop) o["loop"] = true;

    if (f.stabilization.enabled) {
        QJsonObject s;
        s["shakiness"] = f.stabilization.shakiness;
        s["accuracy"] = f.stabilization.accuracy;
        s["smoothing"] = f.stabilization.smoothing;
        s["zoom"] = f.stabilization.zoom;
        s["interpol"] = f.stabilization.interpol;
        o["stabilization"] = s;
    }

    if (f.grid.enabled) {
        QJsonObject g;
        g["otherFile"] = QString::fromStdString(f.grid.otherFile);
        g["vertical"] = f.grid.vertical;
        o["grid"] = g;
    }

    return o;
}

bool filterFromJson(const QJsonObject& o, FilterModel& f, QString* error) {
    FilterModel m;   // all defaults = no filters

    auto rd = [](const QJsonObject& obj, const char* k, double d) { return obj.contains(k) ? obj.value(k).toDouble(d) : d; };
    auto ri = [](const QJsonObject& obj, const char* k, int d) { return obj.contains(k) ? obj.value(k).toInt(d) : d; };
    auto rb = [](const QJsonObject& obj, const char* k, bool d) { return obj.contains(k) ? obj.value(k).toBool(d) : d; };
    auto rs = [](const QJsonObject& obj, const char* k) { return obj.value(k).toString().toStdString(); };

    if (o.contains("trim") && o["trim"].isObject()) {
        const QJsonObject t = o["trim"].toObject();
        m.trim = TrimRange{rd(t, "start", 0.0), rd(t, "end", 0.0)};
    }
    if (o.contains("multiTrim") && o["multiTrim"].isArray()) {
        for (const QJsonValue& v : o["multiTrim"].toArray()) {
            if (m.multiTrim.size() >= 2000) break;
            const QJsonObject t = v.toObject();
            m.multiTrim.push_back(TrimRange{std::max(0.0, rd(t, "start", 0.0)),
                                            std::max(0.0, rd(t, "end", 0.0))});
        }
    }
    if (o.contains("crop") && o["crop"].isObject()) {
        const QJsonObject c = o["crop"].toObject();
        m.crop.enabled = rb(c, "enabled", true);   // presence implies active
        m.crop.x = ri(c, "x", 0); m.crop.y = ri(c, "y", 0);
        m.crop.w = ri(c, "w", 0); m.crop.h = ri(c, "h", 0);
    }
    if (o.contains("dynamicCrop") && o["dynamicCrop"].isObject()) {
        const QJsonObject d = o["dynamicCrop"].toObject();
        m.dynamicCrop.enabled = rb(d, "enabled", true);
        m.dynamicCrop.w = ri(d, "w", 0); m.dynamicCrop.h = ri(d, "h", 0);
        for (const QJsonValue& v : d["keyframes"].toArray()) {
            const QJsonObject k = v.toObject();
            m.dynamicCrop.keyframes.push_back(KeyframeCrop{rd(k, "time", 0.0), ri(k, "x", 0), ri(k, "y", 0), ri(k, "w", 0), ri(k, "h", 0)});
        }
    }
    if (o.contains("resize") && o["resize"].isObject()) {
        const QJsonObject r = o["resize"].toObject();
        m.resizeEnabled = rb(r, "enabled", true);
        m.resizeW = std::clamp(ri(r, "w", 0), 0, 32768);
        m.resizeH = std::clamp(ri(r, "h", 0), 0, 32768);
        m.resizeAlgo = safeAlgo(rs(r, "algo"));
    }
    {
        int rot = ((ri(o, "rotateDegrees", 0) % 360) + 360) % 360;
        m.rotateDegrees = (rot == 90 || rot == 180 || rot == 270) ? rot : 0;
    }
    if (o.contains("rateFps")) m.rateFps = std::clamp(ri(o, "rateFps", 0), 1, 1000);

    if (o.contains("dynamicRate") && o["dynamicRate"].isObject()) {
        const QJsonObject d = o["dynamicRate"].toObject();
        m.dynamicRateEnabled = rb(d, "enabled", true);
        m.dynamicRateSmoothing = rb(d, "smoothing", true);
        m.dynamicRateSmoothingOverlap = rd(d, "overlap", 0.5);
        for (const QJsonValue& v : d["keyframes"].toArray()) {
            if (m.dynamicRate.size() >= 2000) break;
            const QJsonObject k = v.toObject();
            m.dynamicRate.push_back(KeyframeRate{std::max(0.0, rd(k, "time", 0.0)),
                                                 std::clamp(rd(k, "rate", 1.0), 0.01, 1000.0)});
        }
        m.dynamicRateSmoothingOverlap = std::clamp(m.dynamicRateSmoothingOverlap, 0.0, 60.0);
    }
    if (o.contains("caption") && o["caption"].isObject()) {
        const QJsonObject c = o["caption"].toObject();
        m.caption.enabled = rb(c, "enabled", true);
        m.caption.text = rs(c, "text");
        if (m.caption.text.size() > 5000) m.caption.text.resize(5000);
        m.caption.fontFamily = rs(c, "fontFamily"); if (m.caption.fontFamily.empty()) m.caption.fontFamily = "sans";
        m.caption.fontSize = std::clamp(ri(c, "fontSize", 24), 1, 1000);
        m.caption.x = std::clamp(ri(c, "x", 0), -100000, 100000);
        m.caption.y = std::clamp(ri(c, "y", 0), -100000, 100000);
        m.caption.start = std::max(0.0, rd(c, "start", 0.0));
        m.caption.end = std::max(0.0, rd(c, "end", 0.0));
        m.caption.color = safeColor(rs(c, "color"), "white");
        m.caption.borderColor = safeColor(rs(c, "borderColor"), "black");
    }
    if (o.contains("subtitle") && o["subtitle"].isObject()) {
        const QJsonObject s = o["subtitle"].toObject();
        m.subtitle.enabled = rb(s, "enabled", true);
        m.subtitle.streamIndex = std::clamp(ri(s, "streamIndex", -1), -1, 100000);
        m.subtitle.path = safePath(rs(s, "path"));
        m.subtitle.burnIn = rb(s, "burnIn", true);
    }
    if (o.contains("overlay") && o["overlay"].isObject()) {
        const QJsonObject v = o["overlay"].toObject();
        m.overlay.enabled = rb(v, "enabled", true);
        m.overlay.imagePath = safePath(rs(v, "imagePath"));
        m.overlay.x = std::clamp(ri(v, "x", 0), -100000, 100000);
        m.overlay.y = std::clamp(ri(v, "y", 0), -100000, 100000);
        m.overlay.start = std::max(0.0, rd(v, "start", 0.0));
        m.overlay.end = std::max(0.0, rd(v, "end", 0.0));
    }
    if (o.contains("fade") && o["fade"].isObject()) {
        const QJsonObject v = o["fade"].toObject();
        m.fade.enabled = rb(v, "enabled", true);
        m.fade.inDuration = rd(v, "in", 0.0); m.fade.outDuration = rd(v, "out", 0.0);
    }
    if (o.contains("eq") && o["eq"].isObject()) {
        const QJsonObject v = o["eq"].toObject();
        m.eq.enabled = rb(v, "enabled", true);
        m.eq.gamma = rd(v, "gamma", 1.0);
        m.eq.saturation = rd(v, "saturation", 1.0);
        m.eq.contrast = rd(v, "contrast", 1.0);
    }
    m.denoise = rb(o, "denoise", false);
    m.denoiseStrength = std::clamp(ri(o, "denoiseStrength", 0), 0, 2);
    m.deinterlace = rb(o, "deinterlace", false);
    m.delayAudioMs = std::clamp(ri(o, "delayAudioMs", 0), 0, 600000);
    m.reverse = rb(o, "reverse", false);
    m.loop = rb(o, "loop", false);

    if (o.contains("stabilization") && o["stabilization"].isObject()) {
        const QJsonObject s = o["stabilization"].toObject();
        m.stabilization.enabled = rb(s, "enabled", true);
        m.stabilization.shakiness = std::clamp(ri(s, "shakiness", 5), 1, 10);
        m.stabilization.accuracy = std::clamp(ri(s, "accuracy", 15), 1, 15);
        m.stabilization.smoothing = std::clamp(ri(s, "smoothing", 10), 1, 30);
        m.stabilization.zoom = std::clamp(ri(s, "zoom", 0), 0, 20);
        m.stabilization.interpol = std::clamp(ri(s, "interpol", 2), 0, 3);
    }
    if (o.contains("grid") && o["grid"].isObject()) {
        const QJsonObject g = o["grid"].toObject();
        m.grid.enabled = rb(g, "enabled", true);
        m.grid.otherFile = safePath(rs(g, "otherFile"));
        m.grid.vertical = rb(g, "vertical", false);
    }

    f = m;
    (void)error;
    return true;
}

} // namespace wc
