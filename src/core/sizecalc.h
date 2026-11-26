#pragma once
// Target-filesize -> bitrate planning.

#include <cstdint>

namespace wc {

struct SizePlan {
    int videoKbps = 0;
    int audioKbps = 0;
    bool feasible = false;   // false if the duration/inputs make it impossible
};

// Given a clip duration, a target size in bytes and an audio bitrate, choose a
// video bitrate so that the muxed result lands near the target. A small fixed
// overhead factor accounts for container overhead. Mirrors the maths used by the
// original ("get specific filesize") feature.
SizePlan planForSize(double durationSeconds, long long targetBytes, int audioKbps);

} // namespace wc
