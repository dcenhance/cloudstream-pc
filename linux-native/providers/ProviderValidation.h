#pragma once

#include <QString>

namespace CloudStream {

enum class ProviderValidationResult {
    Runnable,
    UtilityOnly,
    RequiresConfiguration,
    AndroidOnly,
    Failed,
};

class ProviderValidation final {
public:
    static ProviderValidationResult classify(int exitCode, int providerCount, const QString &errorText);
};

} // namespace CloudStream
