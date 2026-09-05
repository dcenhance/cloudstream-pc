#pragma once

#include <QtGlobal>

namespace CloudStream {

class ProviderDiscoveryGeneration final {
public:
    quint64 begin() { return ++generation; }
    bool isCurrent(quint64 candidate) const { return candidate == generation; }

private:
    quint64 generation = 0;
};

} // namespace CloudStream
