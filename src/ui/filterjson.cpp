#include "ui/filterjson.h"

#include <QJsonArray>
#include <QJsonValue>

namespace wc {

namespace {
QJsonObject trimObj(const TrimRange& t) {
    QJsonObject o; o["start"] = t.start; o["end"] = t.end; return o;
}
}

QJsonObject filterToJson(const FilterModel& f) {
    QJsonObject o;

    if (f.trim) o["trim"] = trimObj(*f.trim);
    if (!f.multiTrim.empty()) {
        QJsonArray a;
        for (const auto& t : f.multiTrim) a.append(trimObj(t));
        o["multiTrim"] = a;
    }

    { QJsonObject c; c["enabled"] = f.crop.enabled; c["x"] = f.crop.x; c["y"] = f.crop.y;
      c["w"] = f.crop.w; c["h"] = f.crop.h; o["crop"] = c; }

    if (f.dynamicCrop.enabled || !f.dynamicCrop.keyframes.empty()) {
        QJsonObject d; d["enabled"] = f.dynamicCrop.enabled; d["w"] = f.dynamicCrop.w; d["h"] = f.dynamicCrop.h;
        QJsonArray a;
        for (const auto& k : f.dynamicCrop.keyframes) {
            QJsonObject ko; ko["time"] = k.time; ko["x"] = k.x; ko["y"] = k.y; ko["w"] = k.w; ko["h"] = k.h;
            a.append(ko);
        }
        d["keyframes"] = a; o["dynamicCrop"] = d;
    }

    { QJsonObject r; r["enabled"] = f.resizeEnabled; r["w"] = f.resizeW; r["h"] = f.resizeH;
      r["algo"] = QString::fromStdString(f.resizeAlgo); o["resize"] = r; }

    o["rotateDegrees"] = f.rotateDegrees;
    if (f.rateFps) o["rateFps"] = *f.rateFps;

    { QJsonObject d; d["enabled"] = f.dynamicRateEnabled;
      d["smoothing"] = f.dynamicRateSmoothing; d["overlap"] = f.dynamicRateSmoothingOverlap;
      QJsonArray a;
      for (const auto& k : f.dynamicRate) { QJsonObject ko; ko["time"] = k.time; ko["rate"] = k.rate; a.append(ko); }
      d["keyframes"] = a; o["dynamicRate"] = d; }

    { QJsonObject c; c["enabled"] = f.caption.enabled; c["text"] = QString::fromStdString(f.caption.text);
      c["fontFamily"] = QString::fromStdString(f.caption.fontFamily); c["fontSize"] = f.caption.fontSize;
      c["x"] = f.caption.x; c["y"] = f.caption.y; c["start"] = f.caption.start; c["end"] = f.caption.end;
      c["color"] = QString::fromStdString(f.caption.color); c["borderColor"] = QString::fromStdString(f.caption.borderColor);
      o["caption"] = c; }

    { QJsonObject s; s["enabled"] = f.subtitle.enabled; s["streamIndex"] = f.subtitle.streamIndex;
      s["path"] = QString::fromStdString(f.subtitle.path); s["burnIn"] = f.subtitle.burnIn; o["subtitle"] = s; }

    { QJsonObject v; v["enabled"] = f.overlay.enabled; v["imagePath"] = QString::fromStdString(f.overlay.imagePath);
      v["x"] = f.overlay.x; v["y"] = f.overlay.y; v["start"] = f.overlay.start; v["end"] = f.overlay.end; o["overlay"] = v; }

    { QJsonObject v; v["enabled"] = f.fade.enabled; v["in"] = f.fade.inDuration; v["out"] = f.fade.outDuration; o["fade"] = v; }

    { QJsonObject v; v["enabled"] = f.eq.enabled; v["gamma"] = f.eq.gamma; v["saturation"] = f.eq.saturation;
      v["contrast"] = f.eq.contrast; o["eq"] = v; }

    o["denoise"] = f.denoise; o["denoiseStrength"] = f.denoiseStrength;
    o["deinterlace"] = f.deinterlace; o["delayAudioMs"] = f.delayAudioMs;
    o["reverse"] = f.reverse; o["loop"] = f.loop;

    { QJsonObject s; s["enabled"] = f.stabilization.enabled; s["shakiness"] = f.stabilization.shakiness;
      s["accuracy"] = f.stabilization.accuracy; s["smoothing"] = f.stabilization.smoothing;
      s["zoom"] = f.stabilization.zoom; s["interpol"] = f.stabilization.interpol; o["stabilization"] = s; }

    { QJsonObject g; g["enabled"] = f.grid.enabled; g["otherFile"] = QString::fromStdString(f.grid.otherFile);
      g["vertical"] = f.grid.vertical; o["grid"] = g; }

    return o;
}

bool filterFromJson(const QJsonObject& o, FilterModel& f, QString* error) {
    FilterModel m;   // start from defaults, then override known keys

    auto rd = [](const QJsonObject& o, const char* k, double d) { return o.contains(k) ? o.value(k).toDouble(d) : d; };
    auto ri = [](const QJsonObject& o, const char* k, int d) { return o.contains(k) ? o.value(k).toInt(d) : d; };
    auto rb = [](const QJsonObject& o, const char* k, bool d) { return o.contains(k) ? o.value(k).toBool(d) : d; };
    auto rs = [](const QJsonObject& o, const char* k) { return o.value(k).toString().toStdString(); };

    if (o.contains("trim") && o["trim"].isObject()) {
        const QJsonObject t = o["trim"].toObject();
        m.trim = TrimRange{rd(t, "start", 0.0), rd(t, "end", 0.0)};
    }
    if (o.contains("multiTrim") && o["multiTrim"].isArray()) {
        for (const QJsonValue& v : o["multiTrim"].toArray()) {
            const QJsonObject t = v.toObject();
            m.multiTrim.push_back(TrimRange{rd(t, "start", 0.0), rd(t, "end", 0.0)});
        }
    }
    if (o.contains("crop")) {
        const QJsonObject c = o["crop"].toObject();
        m.crop.enabled = rb(c, "enabled", false); m.crop.x = ri(c, "x", 0); m.crop.y = ri(c, "y", 0);
        m.crop.w = ri(c, "w", 0); m.crop.h = ri(c, "h", 0);
    }
    if (o.contains("dynamicCrop")) {
        const QJsonObject d = o["dynamicCrop"].toObject();
        m.dynamicCrop.enabled = rb(d, "enabled", false);
        m.dynamicCrop.w = ri(d, "w", 0); m.dynamicCrop.h = ri(d, "h", 0);
        for (const QJsonValue& v : d["keyframes"].toArray()) {
            const QJsonObject k = v.toObject();
            m.dynamicCrop.keyframes.push_back(KeyframeCrop{rd(k, "time", 0.0), ri(k, "x", 0), ri(k, "y", 0), ri(k, "w", 0), ri(k, "h", 0)});
        }
    }
    if (o.contains("resize")) {
        const QJsonObject r = o["resize"].toObject();
        m.resizeEnabled = rb(r, "enabled", false); m.resizeW = ri(r, "w", 0); m.resizeH = ri(r, "h", 0);
        m.resizeAlgo = rs(r, "algo"); if (m.resizeAlgo.empty()) m.resizeAlgo = "bicubic";
    }
    m.rotateDegrees = ri(o, "rotateDegrees", 0);
    if (o.contains("rateFps")) m.rateFps = ri(o, "rateFps", 0);

    if (o.contains("dynamicRate")) {
        const QJsonObject d = o["dynamicRate"].toObject();
        m.dynamicRateEnabled = rb(d, "enabled", false);
        m.dynamicRateSmoothing = rb(d, "smoothing", true);
        m.dynamicRateSmoothingOverlap = rd(d, "overlap", 0.5);
        for (const QJsonValue& v : d["keyframes"].toArray()) {
            const QJsonObject k = v.toObject();
            m.dynamicRate.push_back(KeyframeRate{rd(k, "time", 0.0), rd(k, "rate", 1.0)});
        }
    }
    if (o.contains("caption")) {
        const QJsonObject c = o["caption"].toObject();
        m.caption.enabled = rb(c, "enabled", false); m.caption.text = rs(c, "text");
        m.caption.fontFamily = rs(c, "fontFamily"); if (m.caption.fontFamily.empty()) m.caption.fontFamily = "sans";
        m.caption.fontSize = ri(c, "fontSize", 24); m.caption.x = ri(c, "x", 0); m.caption.y = ri(c, "y", 0);
        m.caption.start = rd(c, "start", 0.0); m.caption.end = rd(c, "end", 0.0);
        m.caption.color = rs(c, "color"); if (m.caption.color.empty()) m.caption.color = "white";
        m.caption.borderColor = rs(c, "borderColor"); if (m.caption.borderColor.empty()) m.caption.borderColor = "black";
    }
    if (o.contains("subtitle")) {
        const QJsonObject s = o["subtitle"].toObject();
        m.subtitle.enabled = rb(s, "enabled", false); m.subtitle.streamIndex = ri(s, "streamIndex", -1);
        m.subtitle.path = rs(s, "path"); m.subtitle.burnIn = rb(s, "burnIn", true);
    }
    if (o.contains("overlay")) {
        const QJsonObject v = o["overlay"].toObject();
        m.overlay.enabled = rb(v, "enabled", false); m.overlay.imagePath = rs(v, "imagePath");
        m.overlay.x = ri(v, "x", 0); m.overlay.y = ri(v, "y", 0);
        m.overlay.start = rd(v, "start", 0.0); m.overlay.end = rd(v, "end", 0.0);
    }
    if (o.contains("fade")) {
        const QJsonObject v = o["fade"].toObject();
        m.fade.enabled = rb(v, "enabled", false); m.fade.inDuration = rd(v, "in", 0.0); m.fade.outDuration = rd(v, "out", 0.0);
    }
    if (o.contains("eq")) {
        const QJsonObject v = o["eq"].toObject();
        m.eq.enabled = rb(v, "enabled", false); m.eq.gamma = rd(v, "gamma", 1.0);
        m.eq.saturation = rd(v, "saturation", 1.0); m.eq.contrast = rd(v, "contrast", 1.0);
    }
    m.denoise = rb(o, "denoise", false); m.denoiseStrength = ri(o, "denoiseStrength", 0);
    m.deinterlace = rb(o, "deinterlace", false); m.delayAudioMs = ri(o, "delayAudioMs", 0);
    m.reverse = rb(o, "reverse", false); m.loop = rb(o, "loop", false);
    if (o.contains("stabilization")) {
        const QJsonObject s = o["stabilization"].toObject();
        m.stabilization.enabled = rb(s, "enabled", false); m.stabilization.shakiness = ri(s, "shakiness", 5);
        m.stabilization.accuracy = ri(s, "accuracy", 15); m.stabilization.smoothing = ri(s, "smoothing", 10);
        m.stabilization.zoom = ri(s, "zoom", 0); m.stabilization.interpol = ri(s, "interpol", 2);
    }
    if (o.contains("grid")) {
        const QJsonObject g = o["grid"].toObject();
        m.grid.enabled = rb(g, "enabled", false); m.grid.otherFile = rs(g, "otherFile"); m.grid.vertical = rb(g, "vertical", false);
    }

    f = m;
    (void)error;
    return true;
}

} // namespace wc
