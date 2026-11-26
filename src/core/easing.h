#pragma once
// Easing expressions for keyframed ("dynamic") filters. Produces ffmpeg
// expression fragments that interpolate a value from `a` to `b` over logical
// time using the `t` variable. Ported from WebMConverter.Utility.GetEasingExpression.

#include <string>

namespace wc {

// easingFunc: one of "linear", "easeIn", "easeOut", "easeInOut",
//             "quadIn", "quadOut", "quadInOut", "cubicIn", "cubicOut", "cubicInOut".
// a,b: start/end values. p: duration of the segment in seconds.
// Returns an ffmpeg expression valid inside a filter where `t` is available.
std::string easingExpression(const std::string& easingFunc, double a, double b, double p);

// Linear interpolation used by the piecewise keyframe helpers.
double lerp(double a, double b, double f);

} // namespace wc
