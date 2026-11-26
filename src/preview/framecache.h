#pragma once
// Bounded LRU cache of decoded preview frames (keyed by frame number).

#include <QImage>
#include <QCache>

namespace wc {

class FrameCache {
public:
    explicit FrameCache(int maxCostBytes = 200 * 1024 * 1024);

    bool contains(int frame) const { return cache_.contains(frame); }
    QImage get(int frame) const {
        if (QImage* p = cache_.object(frame)) return *p;
        return QImage();
    }
    void insert(int frame, const QImage& img);
    void setMaxCost(int bytes) { cache_.setMaxCost(bytes); }
    void clear() { cache_.clear(); }

private:
    QCache<int, QImage> cache_;
};

} // namespace wc
