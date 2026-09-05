#pragma once

#include <QObject>
#include <QProcess>

#include <functional>
#include <memory>

namespace CloudStream {

class ProcessCompletion final {
public:
    using Callback = std::function<void(int, QProcess::ExitStatus, bool)>;

    static void watch(QProcess *process, QObject *context, Callback callback) {
        if (!process || !context || !callback) return;
        auto completed = std::make_shared<bool>(false);
        auto invoke = std::make_shared<Callback>(std::move(callback));
        const auto finish = [completed, invoke](int exitCode,
                                                QProcess::ExitStatus exitStatus,
                                                bool failedToStart) {
            if (*completed) return;
            *completed = true;
            (*invoke)(exitCode, exitStatus, failedToStart);
        };
        QObject::connect(
            process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            context, [finish](int exitCode, QProcess::ExitStatus exitStatus) {
                finish(exitCode, exitStatus, false);
            });
        QObject::connect(
            process, &QProcess::errorOccurred, context,
            [finish](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    finish(-1, QProcess::NormalExit, true);
                }
            });
    }
};

} // namespace CloudStream
