#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>

namespace CloudStream {
struct HomeProcessResult {
    QJsonArray sections;
    QString error;
    static HomeProcessResult parse(const QByteArray &output, const QByteArray &diagnostics,
                                  int exitCode, QProcess::ExitStatus exitStatus,
                                  bool startFailure, bool timedOut, const QString &processError) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(output, &parseError);
        QString error;
        if (startFailure) error = "Could not start the provider host: " + processError;
        else if (timedOut) error = "Provider Home request timed out after 20 seconds.";
        else if (exitStatus != QProcess::NormalExit) error = "Provider host crashed while loading Home.";
        else if (exitCode != 0) error = QString("Provider host failed (exit %1).").arg(exitCode);
        else if (parseError.error != QJsonParseError::NoError)
            error = "Provider host returned invalid Home JSON: " + parseError.errorString();
        else if (!document.isArray()) error = "Provider host returned invalid Home JSON: expected an array.";
        if (!error.isEmpty()) {
            // Provider logs may contain markup; the UI displays this as plain text.
            const auto detail = QString::fromUtf8(diagnostics).trimmed().right(2000);
            if (!detail.isEmpty()) error += "\n" + detail;
            return {{}, error};
        }
        return {document.array(), {}};
    }
};
}
