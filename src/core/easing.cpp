#include "easing.h"
#include <cmath>
#include <cstdio>

namespace wc {

double lerp(double a, double b, double f) { return a + (b - a) * f; }

namespace {

// Format a double compactly for embedding in an ffmpeg expression.
std::string num(double v) {
    char buf[64];
    // Trim trailing zeros; keep enough precision for pixel positions.
    std::snprintf(buf, sizeof(buf), "%.5f", v);
    std::string s(buf);
    while (s.size() > 1 && s.back() == '0' && s.find('.') != std::string::npos)
        s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

} // namespace

std::string easingExpression(const std::string& easingFunc, double a, double b, double p) {
    if (p <= 0.0) p = 1.0;
    const std::string A = num(a);
    const std::string B = num(b);
    const std::string P = num(p);
    const std::string span = "(" + B + "-" + A + ")";

    // Progress variable u in [0,1].
    const std::string u = "(min(max(t/" + P + ",0),1))";

    std::string eased;
    if (easingFunc == "linear") {
        eased = u;
    } else if (easingFunc == "easeIn" || easingFunc == "quadIn") {
        eased = "(" + u + "*" + u + ")";
    } else if (easingFunc == "easeOut" || easingFunc == "quadOut") {
        eased = "(1-(1-" + u + ")*(1-" + u + "))";
    } else if (easingFunc == "quadInOut" || easingFunc == "easeInOut") {
        eased = "if(lt(" + u + ",0.5),2*" + u + "*" + u + ",1-2*(1-" + u + ")*(1-" + u + "))";
    } else if (easingFunc == "cubicIn") {
        eased = "(" + u + "*" + u + "*" + u + ")";
    } else if (easingFunc == "cubicOut") {
        eased = "(1-(1-" + u + ")*(1-" + u + ")*(1-" + u + "))";
    } else if (easingFunc == "cubicInOut") {
        eased = "if(lt(" + u + ",0.5),4*" + u + "*" + u + "*" + u +
                ",1-4*(1-" + u + ")*(1-" + u + ")*(1-" + u + "))";
    } else {
        // Unknown easing -> linear.
        eased = u;
    }

    return "(" + A + "+" + span + "*" + eased + ")";
}

} // namespace wc
