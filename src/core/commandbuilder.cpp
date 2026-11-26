#include "commandbuilder.h"
#include "sizecalc.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <sstream>

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

std::vector<std::string> splitWs(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string t;
    while (iss >> t) out.push_back(t);
    return out;
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

// Escape a value for use inside single quotes in a filter argument.
std::string quoteFilterValue(const std::string& v) {
    std::string out = "'";
    for (char c : v) {
        if (c == '\'') out += "\\'";
        else out += c;
    }
    out += "'";
    return out;
}

// Escape text for drawtext.
std::string escapeDrawtext(const std::string& in) {
    std::string out;
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            case ':':  out += "\\:"; break;
            case '%':  out += "\\%"; break;
            default:   out += c; break;
        }
    }
    return out;
}

// Build a piecewise-linear expression in `t` from (time,value) points.
std::string piecewiseLinear(const std::vector<std::pair<double, double>>& pts) {
    if (pts.empty()) return "0";
    if (pts.size() == 1) return num(pts.front().second);

    std::string expr = num(pts.back().second);
    for (int i = static_cast<int>(pts.size()) - 2; i >= 0; --i) {
        const double t0 = pts[i].first;
        const double t1 = pts[i + 1].first;
        const double a = pts[i].second;
        const double b = pts[i + 1].second;
        const double p = (t1 - t0) != 0.0 ? (t1 - t0) : 1.0;
        const std::string seg = "(" + num(a) + "+(" + num(b) + "-" + num(a) + ")*(t-" +
                                num(t0) + ")/" + num(p) + ")";
        expr = "if(lte(t," + num(t1) + ")," + seg + "," + expr + ")";
    }
    return expr;
}

} // namespace

CommandBuilder::CommandBuilder(const VideoInfo& info, const FilterModel& filters,
                               const EncodingOptions& enc)
    : info_(info), f_(filters), e_(enc) {}

bool CommandBuilder::isTwoPass() const { return e_.twoPass; }

std::string CommandBuilder::codecName() const {
    switch (e_.codec) {
        case VideoCodec::VP8:  return "libvpx";
        case VideoCodec::VP9:  return "libvpx-vp9";
        case VideoCodec::H264: return "libx264";
        case VideoCodec::H265: return "libx265";
    }
    return "libvpx-vp9";
}

std::string CommandBuilder::containerFormat() const {
    return (e_.codec == VideoCodec::VP8 || e_.codec == VideoCodec::VP9) ? "webm" : "mp4";
}

std::string CommandBuilder::pixelFormat() const {
    if (e_.hdr) return "yuv420p10le";
    if (e_.useAlpha) return "yuva420p";
    return "yuv420p";
}

std::string CommandBuilder::audioEncoder() const {
    if (containerFormat() == "webm") {
        if (e_.audioCodec == AudioCodec::Vorbis) return "libvorbis";
        return "libopus";
    }
    return "aac";
}

double CommandBuilder::clipDuration() const {
    if (!f_.multiTrim.empty()) {
        double total = 0.0;
        for (const auto& r : f_.multiTrim) {
            if (r.end > r.start) total += (r.end - r.start);
        }
        return total > 0.0 ? total : info_.durationSeconds;
    }
    if (f_.trim && f_.trim->end > f_.trim->start)
        return f_.trim->end - f_.trim->start;
    return info_.durationSeconds;
}

int CommandBuilder::effectiveAudioKbps() const {
    return e_.disableAudio ? 0 : e_.audioBitrateKbps;
}

int CommandBuilder::effectiveVideoKbps() const {
    if (e_.sizeLimitBytes > 0) {
        SizePlan plan = planForSize(clipDuration(), e_.sizeLimitBytes, effectiveAudioKbps());
        return plan.videoKbps;
    }
    return e_.videoBitrateKbps;
}

bool CommandBuilder::hasVideoFilters() const {
    if (!f_.multiTrim.empty()) return true;
    if (f_.crop.enabled) return true;
    if (f_.dynamicCrop.enabled && f_.dynamicCrop.keyframes.size() >= 2) return true;
    if (f_.resizeEnabled && f_.resizeW > 0 && f_.resizeH > 0) return true;
    if (f_.rotateDegrees % 360 != 0) return true;
    if (f_.dynamicRateEnabled && f_.dynamicRate.size() >= 2) return true;
    if (f_.eq.enabled) return true;
    if (f_.subtitle.enabled && !f_.subtitle.path.empty()) return true;
    if (f_.caption.enabled && !f_.caption.text.empty()) return true;
    if (f_.overlay.enabled && !f_.overlay.imagePath.empty()) return true;
    if (f_.fade.enabled && (f_.fade.inDuration > 0 || f_.fade.outDuration > 0)) return true;
    if (f_.denoise) return true;
    if (f_.deinterlace) return true;
    if (f_.reverse) return true;
    if (f_.loop) return true;
    if (f_.stabilization.enabled) return true;
    return false;
}

bool CommandBuilder::needsFilterComplex() const { return hasVideoFilters(); }

std::string CommandBuilder::videoFilterGraph() const {
    if (!needsFilterComplex()) return "";

    std::string graph;
    std::string cur = "[0:v]";

    // --- Multi-trim first (split + trim + concat) ---
    if (!f_.multiTrim.empty()) {
        const size_t n = f_.multiTrim.size();
        std::string labels;
        for (size_t i = 0; i < n; ++i) labels += "[s" + std::to_string(i) + "]";
        graph += cur + "split=" + std::to_string(n) + labels + ";";
        std::string concatIn;
        for (size_t i = 0; i < n; ++i) {
            const auto& r = f_.multiTrim[i];
            const std::string seg = "[s" + std::to_string(i) + "]trim=start=" + num(r.start) +
                                    ":end=" + num(r.end) + ",setpts=PTS-STARTPTS[t" +
                                    std::to_string(i) + "];";
            graph += seg;
            concatIn += "[t" + std::to_string(i) + "]";
        }
        graph += concatIn + "concat=n=" + std::to_string(n) + ":v=1:a=0[vt];";
        cur = "[vt]";
    }

    // --- Simple linear chain ---
    std::vector<std::string> chain;
    if (f_.stabilization.enabled) {
        const auto& s = f_.stabilization;
        chain.push_back("vidstabtransform=input=transforms.trf:interpol=" +
                        std::to_string(s.interpol) + ":zoom=" + std::to_string(s.zoom) +
                        ":smoothing=" + std::to_string(s.smoothing) +
                        ":maxangle=0:maxshift=-1");
        chain.push_back("unsharp=5:5:0.8:3:3:0.4");
    }
    if (f_.denoise) {
        static const char* presets[] = {"hqdn3d=1.5:1.5:6:6", "hqdn3d=2:2:8:8", "hqdn3d=3:3:10:10"};
        int idx = f_.denoiseStrength;
        if (idx < 0) idx = 0;
        if (idx > 2) idx = 2;
        chain.push_back(presets[idx]);
    }
    if (f_.deinterlace) chain.emplace_back("yadif");

    if (f_.crop.enabled && f_.crop.w > 0 && f_.crop.h > 0) {
        chain.push_back("crop=" + std::to_string(f_.crop.w) + ":" + std::to_string(f_.crop.h) +
                        ":" + std::to_string(f_.crop.x) + ":" + std::to_string(f_.crop.y));
    }

    if (f_.dynamicCrop.enabled && f_.dynamicCrop.keyframes.size() >= 2) {
        std::vector<std::pair<double, double>> xs, ys;
        for (const auto& k : f_.dynamicCrop.keyframes) {
            xs.emplace_back(k.time, static_cast<double>(k.x));
            ys.emplace_back(k.time, static_cast<double>(k.y));
        }
        const int w = f_.dynamicCrop.w > 0 ? f_.dynamicCrop.w : f_.dynamicCrop.keyframes.front().w;
        const int h = f_.dynamicCrop.h > 0 ? f_.dynamicCrop.h : f_.dynamicCrop.keyframes.front().h;
        chain.push_back("crop=" + std::to_string(w) + ":" + std::to_string(h) +
                        ":x='" + piecewiseLinear(xs) + "':y='" + piecewiseLinear(ys) + "'");
    }

    if (f_.resizeEnabled && f_.resizeW > 0 && f_.resizeH > 0) {
        std::string algo = f_.resizeAlgo.empty() ? "bicubic" : f_.resizeAlgo;
        chain.push_back("scale=" + std::to_string(f_.resizeW) + ":" +
                        std::to_string(f_.resizeH) + ":flags=" + algo);
    }

    if (f_.rotateDegrees % 360 != 0) {
        int deg = ((f_.rotateDegrees % 360) + 360) % 360;
        if (deg == 90) chain.emplace_back("transpose=1");
        else if (deg == 270) chain.emplace_back("transpose=2");
        else if (deg == 180) { chain.emplace_back("transpose=1"); chain.emplace_back("transpose=1"); }
    }

    if (f_.dynamicRateEnabled && f_.dynamicRate.size() >= 2) {
        std::vector<std::pair<double, double>> rates;
        for (const auto& k : f_.dynamicRate) rates.emplace_back(k.time, k.rate);
        chain.push_back("setpts='(PTS)/(" + piecewiseLinear(rates) + ")'");
    }

    if (f_.eq.enabled) {
        chain.push_back("eq=gamma=" + num(f_.eq.gamma) + ":saturation=" + num(f_.eq.saturation) +
                        ":contrast=" + num(f_.eq.contrast));
    }

    if (f_.subtitle.enabled && !f_.subtitle.path.empty()) {
        chain.push_back("subtitles=" + quoteFilterValue(f_.subtitle.path));
    }

    if (f_.caption.enabled && !f_.caption.text.empty()) {
        std::string d = "drawtext=text='" + escapeDrawtext(f_.caption.text) + "'";
        d += ":fontcolor=" + f_.caption.color;
        d += ":fontsize=" + std::to_string(f_.caption.fontSize);
        d += ":borderw=2:bordercolor=" + f_.caption.borderColor;
        d += ":x=" + std::to_string(f_.caption.x) + ":y=" + std::to_string(f_.caption.y);
        if (f_.caption.end > f_.caption.start) {
            d += ":enable='between(t," + num(f_.caption.start) + "," + num(f_.caption.end) + ")'";
        }
        chain.push_back(d);
    }

    if (f_.fade.enabled) {
        const double dur = clipDuration();
        if (f_.fade.inDuration > 0)
            chain.push_back("fade=t=in:st=0:d=" + num(f_.fade.inDuration));
        if (f_.fade.outDuration > 0) {
            double st = dur - f_.fade.outDuration;
            if (st < 0) st = 0;
            chain.push_back("fade=t=out:st=" + num(st) + ":d=" + num(f_.fade.outDuration));
        }
    }

    if (f_.reverse && !f_.loop) chain.emplace_back("reverse");

    if (!chain.empty()) {
        graph += cur + join(chain, ",") + "[v0];";
        cur = "[v0]";
    }

    // --- Overlay (movie source) ---
    if (f_.overlay.enabled && !f_.overlay.imagePath.empty()) {
        graph += "movie=" + quoteFilterValue(f_.overlay.imagePath) + "[wm];" + cur +
                 "[wm]overlay=" + std::to_string(f_.overlay.x) + ":" + std::to_string(f_.overlay.y) +
                 ":shortest=1[v1];";
        cur = "[v1]";
    }

    // --- Loop (forward + reverse concat, repeated) ---
    if (f_.loop) {
        const long long frames = std::max<long long>(1, info_.frameCount());
        graph += cur + "split[lf][lb];[lb]reverse[rv];[lf][rv]concat=n=2:v=1[lp];[lp]loop=loop=1:size=" +
                 std::to_string(frames) + "[vout]";
        return graph;
    }

    // Normalise the terminal label to [vout].
    if (cur != "[vout]") {
        graph += cur + "copy[vout]";
    }
    return graph;
}

std::string CommandBuilder::rateControlArgs() const {
    std::vector<std::string> a;
    if (e_.sizeLimitBytes > 0 || e_.rateControl == RateControl::Constant) {
        a.push_back("-b:v");
        a.push_back(std::to_string(effectiveVideoKbps()) + "k");
        if (containerFormat() == "webm") { a.push_back("-qcomp"); a.push_back("0"); }
        if (e_.sizeLimitBytes > 0) {
            a.push_back("-fs");
            a.push_back(std::to_string(e_.sizeLimitBytes));
        }
    } else {
        // variable quality
        if (e_.codec == VideoCodec::VP8 || e_.codec == VideoCodec::VP9) {
            a.push_back("-qmin"); a.push_back("0");
            a.push_back("-crf"); a.push_back(std::to_string(e_.crf));
            a.push_back("-qmax"); a.push_back("63");
            a.push_back("-qcomp"); a.push_back("1");
            a.push_back("-b:v"); a.push_back("0");
        } else {
            a.push_back("-crf"); a.push_back(std::to_string(e_.crf));
        }
    }
    return join(a, " ");
}

std::string CommandBuilder::videoCodecArgs() const {
    std::vector<std::string> a;
    a.push_back("-c:v");
    a.push_back(codecName());

    for (const auto& t : splitWs(rateControlArgs())) a.push_back(t);

    a.push_back("-pix_fmt");
    a.push_back(pixelFormat());

    if (e_.hq) {
        a.push_back("-lag-in-frames"); a.push_back("16");
        if (!e_.useAlpha) { a.push_back("-auto-alt-ref"); a.push_back("1"); }
    }
    if (e_.codec == VideoCodec::VP9) {
        a.push_back("-tile-columns"); a.push_back("1");
        a.push_back("-row-mt"); a.push_back("1");
    }
    if (e_.threads > 0) { a.push_back("-threads"); a.push_back(std::to_string(e_.threads)); }
    if (e_.slices > 0) { a.push_back("-slices"); a.push_back(std::to_string(e_.slices)); }

    // Frame rate: explicit encoding target wins, else the "change rate" filter.
    int outFps = e_.targetFps;
    if (outFps <= 0 && f_.rateFps && *f_.rateFps > 0) outFps = *f_.rateFps;
    if (outFps > 0) { a.push_back("-r"); a.push_back(std::to_string(outFps)); }

    if (e_.hdr) {
        a.push_back("-color_primaries"); a.push_back("bt2020");
        a.push_back("-color_trc"); a.push_back("smpte2084");
        a.push_back("-colorspace"); a.push_back("bt2020nc");
    }
    return join(a, " ");
}

std::string CommandBuilder::audioFilterArgs() const {
    std::vector<std::string> af;
    if (e_.normalizeAudio) af.emplace_back("loudnorm=I=-16:TP=-1.5:LRA=11");
    if (e_.amplifyAudio) af.emplace_back("volume=3dB");
    if (f_.delayAudioMs > 0) {
        af.push_back("adelay=" + std::to_string(f_.delayAudioMs) + "|" + std::to_string(f_.delayAudioMs));
    }
    if (f_.dynamicRateEnabled && f_.dynamicRate.size() >= 2) {
        // Approximate constant atempo from the mean speed multiplier.
        double sum = 0.0;
        for (const auto& k : f_.dynamicRate) sum += k.rate;
        double mean = sum / f_.dynamicRate.size();
        if (mean > 0.01 && std::fabs(mean - 1.0) > 0.001) {
            double r = std::clamp(mean, 0.5, 2.0);
            af.push_back("atempo=" + num(r));
        }
    }
    return join(af, ",");
}

void CommandBuilder::appendInputArgs(std::vector<std::string>& a, const std::string& inputPath) const {
    const bool multi = !f_.multiTrim.empty();
    if (!multi && f_.trim && f_.trim->start > 0.0) {
        a.push_back("-ss");
        a.push_back(num(f_.trim->start));
    }
    a.push_back("-i");
    a.push_back(inputPath);
    if (!multi && f_.trim && f_.trim->end > f_.trim->start) {
        a.push_back("-t");
        a.push_back(num(f_.trim->end - f_.trim->start));
    }
}

void CommandBuilder::appendVideoArgs(std::vector<std::string>& a) const {
    if (needsFilterComplex()) {
        a.push_back("-filter_complex");
        a.push_back(videoFilterGraph());
        a.push_back("-map");
        a.push_back("[vout]");
    } else {
        a.push_back("-map");
        a.push_back("0:v:0");
    }
    for (const auto& t : splitWs(videoCodecArgs())) a.push_back(t);
}

void CommandBuilder::appendAudioArgs(std::vector<std::string>& a) const {
    if (e_.disableAudio) { a.push_back("-an"); return; }
    a.push_back("-map");
    a.push_back("0:a:0?");
    a.push_back("-c:a");
    a.push_back(audioEncoder());
    a.push_back("-b:a");
    a.push_back(std::to_string(effectiveAudioKbps()) + "k");
    if (e_.monoAudio) { a.push_back("-ac"); a.push_back("1"); }
    const std::string af = audioFilterArgs();
    if (!af.empty()) { a.push_back("-af"); a.push_back(af); }
}

void CommandBuilder::appendOutputArgs(std::vector<std::string>& a, const std::string& outputPath) const {
    a.push_back("-f");
    a.push_back(containerFormat());
    a.push_back("-y");
    if (!e_.title.empty()) {
        a.push_back("-metadata");
        a.push_back("title=" + e_.title);
    }
    a.push_back(outputPath);
}

std::vector<std::string> CommandBuilder::buildGridArgs(const std::string& inputPath,
                                                       const std::string& outputPath) const {
    std::vector<std::string> a;
    a.push_back("-i"); a.push_back(inputPath);
    a.push_back("-i"); a.push_back(f_.grid.otherFile);
    a.push_back("-filter_complex");
    a.push_back(f_.grid.vertical ? "vstack" : "hstack");
    a.push_back("-c:v"); a.push_back(codecName());
    a.push_back("-pix_fmt"); a.push_back(pixelFormat());
    a.push_back("-crf"); a.push_back("16");
    if (containerFormat() == "webm") { a.push_back("-qcomp"); a.push_back("1"); a.push_back("-b:v"); a.push_back("0"); }
    if (e_.disableAudio) { a.push_back("-an"); }
    a.push_back("-f"); a.push_back(containerFormat());
    a.push_back("-y");
    a.push_back(outputPath);
    return a;
}

std::vector<PassPlan> CommandBuilder::build(const std::string& inputPath,
                                            const std::string& outputPath) const {
    std::vector<PassPlan> passes;

    if (f_.grid.enabled && !f_.grid.otherFile.empty()) {
        PassPlan p;
        p.args = buildGridArgs(inputPath, outputPath);
        passes.push_back(p);
        return passes;
    }

    if (!isTwoPass()) {
        PassPlan p;
        appendInputArgs(p.args, inputPath);
        appendVideoArgs(p.args);
        appendAudioArgs(p.args);
        appendOutputArgs(p.args, outputPath);
        passes.push_back(p);
    } else {
        const std::string passLog = outputPath + ".2pass";

        // Pass 1: analyse only.
        {
            PassPlan p;
            p.isAnalysisPass = true;
            appendInputArgs(p.args, inputPath);
            appendVideoArgs(p.args);
            p.args.push_back("-an");
            p.args.push_back("-pass"); p.args.push_back("1");
            p.args.push_back("-passlogfile"); p.args.push_back(passLog);
            p.args.push_back("-f"); p.args.push_back("null");
            p.args.push_back("-");
            passes.push_back(p);
        }
        // Pass 2: real encode.
        {
            PassPlan p;
            appendInputArgs(p.args, inputPath);
            appendVideoArgs(p.args);
            appendAudioArgs(p.args);
            p.args.push_back("-pass"); p.args.push_back("2");
            p.args.push_back("-passlogfile"); p.args.push_back(passLog);
            appendOutputArgs(p.args, outputPath);
            passes.push_back(p);
        }
    }

    // Stabilization requires a preliminary vid.stab detect pass that writes
    // transforms.trf, consumed by the vidstabtransform filter in the encode pass.
    if (f_.stabilization.enabled) {
        PassPlan det;
        det.isAnalysisPass = true;
        det.args.push_back("-i");
        det.args.push_back(inputPath);
        det.args.push_back("-vf");
        det.args.push_back(std::string("vidstabdetect=stepsize=2:shakiness=") +
                           std::to_string(f_.stabilization.shakiness) + ":accuracy=" +
                           std::to_string(f_.stabilization.accuracy) + ":result=transforms.trf");
        det.args.push_back("-f");
        det.args.push_back("null");
        det.args.push_back("-");
        passes.insert(passes.begin(), det);
    }
    return passes;
}

} // namespace wc
