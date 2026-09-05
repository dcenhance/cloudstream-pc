#include "ArtworkSizing.h"

#include <algorithm>

namespace CloudStream {

QSize ArtworkSizing::posterSize(int width) {
    const auto safeWidth = std::max(1, width);
    return {safeWidth, safeWidth * 3 / 2};
}

QSize ArtworkSizing::backdropSize(int width) {
    const auto safeWidth = std::max(1, width);
    return {safeWidth, safeWidth * 9 / 16};
}

QImage ArtworkSizing::centerCrop(const QImage &source, const QSize &target) {
    if (source.isNull() || !target.isValid() || target.isEmpty()) return {};
    const auto scaled = source.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const auto x = std::max(0, (scaled.width() - target.width()) / 2);
    const auto y = std::max(0, (scaled.height() - target.height()) / 2);
    return scaled.copy(x, y, target.width(), target.height());
}

} // namespace CloudStream
