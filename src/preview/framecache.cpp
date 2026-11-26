#include "preview/framecache.h"

namespace wc {

FrameCache::FrameCache(int maxCostBytes) {
    cache_.setMaxCost(maxCostBytes);
}

void FrameCache::insert(int frame, const QImage& img) {
    if (img.isNull()) return;
    // Approximate cost in bytes; sizeInBytes() is exact for the pixel buffer.
    const int cost = static_cast<int>(img.sizeInBytes());
    cache_.insert(frame, new QImage(img), cost);
}

} // namespace wc
