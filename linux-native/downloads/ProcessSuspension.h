#pragma once

#include <QProcess>
#include <QString>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#elif defined(Q_OS_UNIX)
#include <cerrno>
#include <cstring>
#include <signal.h>
#endif

namespace CloudStream::ProcessSuspension {

// Only controls the directly launched FFmpeg process, not a process tree.
// Windows has no public Win32 whole-process suspend API. Resolve the native
// NT calls at runtime rather than linking ntdll or racing thread enumeration.
// Availability/access failures must be reported, never treated as success.
inline bool setSuspended(QProcess *process, bool suspended, QString *error = nullptr) {
    if (error) error->clear();
    const auto fail = [error](const QString &reason) {
        if (error) *error = reason;
        return false;
    };
    // In particular, never send a POSIX signal to PID 0 (the process group).
    if (!process || process->state() != QProcess::Running || process->processId() <= 0)
        return fail(QStringLiteral("The download process is not running"));
#ifdef Q_OS_WIN
    using ProcessOperation = LONG (NTAPI *)(HANDLE);
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto suspend = ntdll ? reinterpret_cast<ProcessOperation>(
        GetProcAddress(ntdll, "NtSuspendProcess")) : nullptr;
    const auto resume = ntdll ? reinterpret_cast<ProcessOperation>(
        GetProcAddress(ntdll, "NtResumeProcess")) : nullptr;
    // Do not suspend if the matching resume operation is unavailable.
    if (!suspend || !resume)
        return fail(QStringLiteral("Windows process suspension API is unavailable"));
    const auto handle = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE,
                                    static_cast<DWORD>(process->processId()));
    if (!handle)
        return fail(QStringLiteral("Cannot access the download process (Windows error %1)")
                    .arg(GetLastError()));
    const LONG status = (suspended ? suspend : resume)(handle);
    CloseHandle(handle);
    if (status < 0)
        return fail(QStringLiteral("Windows process %1 failed (NTSTATUS 0x%2)")
                    .arg(suspended ? QStringLiteral("suspend") : QStringLiteral("resume"))
                    .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0')));
    return true;
#elif defined(Q_OS_UNIX)
    if (::kill(static_cast<pid_t>(process->processId()), suspended ? SIGSTOP : SIGCONT) == 0)
        return true;
    return fail(QString::fromLocal8Bit(std::strerror(errno)));
#else
    Q_UNUSED(suspended);
    return fail(QStringLiteral("Process suspension is unsupported on this platform"));
#endif
}

} // namespace CloudStream::ProcessSuspension
