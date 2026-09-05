#pragma once

#include <QImage>
#include <QSize>

namespace CloudStream {

class ArtworkSizing final {
public:
    static QSize posterSize(int width);
    static QSize backdropSize(int width);
    static QImage centerCrop(const QImage &source, const QSize &target);
};

} // namespace CloudStream
