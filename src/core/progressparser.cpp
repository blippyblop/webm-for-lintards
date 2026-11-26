#include "progressparser.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace wc {

namespace {

// Find "key=value" style token starting at a word boundary.
bool findField(const std::string& line, const std::string& key, std::string& value) {
    size_t pos = 0;
    while ((pos = line.find(key, pos)) != std::string::npos) {
        // Ensure it's at a boundary (start or preceded by whitespace).
        const bool boundary = pos == 0 || line[pos - 1] == ' ' || line[pos - 1] == '\r';
        if (!boundary) { pos += key.size(); continue; }
        size_t vstart = pos + key.size();
        // skip spaces after '='
        while (vstart < line.size() && line[vstart] == ' ') ++vstart;
        size_t vend = vstart;
        while (vend < line.size() && line[vend] != ' ' && line[vend] != '\r' && line[vend] != '\n')
            ++vend;
        value = line.substr(vstart, vend - vstart);
        return !value.empty();
    }
    return false;
}

} // namespace

bool ProgressParser::parseTimestamp(const std::string& s, double& outSeconds) {
    // Accept HH:MM:SS.xx or MM:SS.xx
    int h = 0, m = 0;
    double sec = 0.0;
    int matched = std::sscanf(s.c_str(), "%d:%d:%lf", &h, &m, &sec);
    if (matched == 3) {
        outSeconds = h * 3600.0 + m * 60.0 + sec;
        return true;
    }
    matched = std::sscanf(s.c_str(), "%d:%lf", &m, &sec);
    if (matched == 2) {
        outSeconds = m * 60.0 + sec;
        return true;
    }
    return false;
}

bool ProgressParser::feed(const std::string& line) {
    bool changed = false;
    EncodeProgress p = last_;
    std::string v;

    if (findField(line, "frame=", v)) {
        long long f = std::atoll(v.c_str());
        if (f != p.frame) { p.frame = f; changed = true; }
    }
    if (findField(line, "fps=", v)) {
        double f = std::atof(v.c_str());
        if (f != p.fps) { p.fps = f; changed = true; }
    }
    if (findField(line, "bitrate=", v)) {
        // May be "1234.5kbits/s"; strip non-numeric tail.
        double b = std::atof(v.c_str());
        if (b != p.bitrateKbps) { p.bitrateKbps = b; changed = true; }
    }
    if (findField(line, "speed=", v)) {
        double s = std::atof(v.c_str());   // "1.23x" -> 1.23
        if (s != p.speed) { p.speed = s; changed = true; }
    }
    if (findField(line, "time=", v) || findField(line, "out_time=", v)) {
        double t = 0.0;
        if (parseTimestamp(v, t) && t != p.timeSeconds) {
            p.timeSeconds = t;
            if (duration_ > 0.0) {
                double pct = (t / duration_) * 100.0;
                if (pct > 100.0) pct = 100.0;
                p.percent = pct;
            }
            changed = true;
        }
    }

    if (changed) last_ = p;
    return changed;
}

} // namespace wc
