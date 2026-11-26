#pragma once
// Builds a single-frame ffmpeg filter chain that approximates how the current
// filters look at a given point in time, for live preview while scrubbing.

#include <string>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

namespace wc {

// Returns an ffmpeg -vf chain (may be empty). Renders geometry/colour filters
// plus captions/overlays at tSeconds. Time-shifting filters (fade) are skipped.
std::string buildPreviewVideoFilter(const FilterModel& filters, const VideoInfo& info,
                                    double tSeconds);

} // namespace wc
