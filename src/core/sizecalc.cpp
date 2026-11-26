#include "sizecalc.h"
#include <cmath>

namespace wc {

SizePlan planForSize(double durationSeconds, long long targetBytes, int audioKbps) {
    SizePlan plan;
    plan.audioKbps = audioKbps;
    if (durationSeconds <= 0.0 || targetBytes <= 0) {
        plan.feasible = false;
        return plan;
    }

    // Total budget in kilobits.
    const double totalKbps = (static_cast<double>(targetBytes) * 8.0 / 1000.0) / durationSeconds;

    // Reserve ~2% headroom for container/muxing overhead, then subtract audio.
    const double usable = totalKbps * 0.98 - audioKbps;

    if (usable < 8.0) {
        plan.feasible = false;
        plan.videoKbps = 8;   // clamp to a floor so the command still runs
        return plan;
    }

    plan.videoKbps = static_cast<int>(std::floor(usable));
    plan.feasible = true;
    return plan;
}

} // namespace wc
