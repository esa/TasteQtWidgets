/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this program. If not, see <https://www.gnu.org/licenses/lgpl-2.1.html>.
*/

#include "tqtw_wire.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>
#include <QTimer>

namespace tqtw_orbitacp {

namespace {
//: The longest line orbit reads. A longer one it drops without a reply.
constexpr int MAX_LINE = 32 * 1024 * 1024;
constexpr int STDERR_LINES = 40;

//: The directory the AppImage is mounted/extracted at, if we run inside one.
QString bundleRoot()
{
    const QString appDir = qEnvironmentVariable("APPDIR");
    if (!appDir.isEmpty()) {
        return appDir;
    }
    const QString image = qEnvironmentVariable("APPIMAGE");
    if (!image.isEmpty()) {
        return QFileInfo(image).absolutePath();
    }
    return QString();
}

/*!
 * The `LD_LIBRARY_PATH` a child process may inherit.

 * Run from an AppImage, the loader is steered to the bundle's own libraries
 * (its Qt, its `lib/x86_64-linux-gnu`, ...) through `LD_LIBRARY_PATH`. A child
 * is often a host program with its own extension modules - orbit is a host
 * Python whose `_ssl` links the host OpenSSL - and handing it the bundled
 * libraries makes it resolve the wrong ones ("version `OPENSSL_3.3.0' not
 * found"). Our `AppRun` saves the value it found in
 * `_ORIGINAL_SC_HOST_LD_LIBRARY_PATH`, so restore that; otherwise drop the
 * paths that point inside the bundle and keep the rest.
 */
QString sanitizedLibraryPath()
{
    if (qEnvironmentVariableIsSet("_ORIGINAL_SC_HOST_LD_LIBRARY_PATH")) {
        return qEnvironmentVariable("_ORIGINAL_SC_HOST_LD_LIBRARY_PATH");
    }
    const QString current = qEnvironmentVariable("LD_LIBRARY_PATH");
    QString bundle = bundleRoot();
    if (current.isEmpty() || bundle.isEmpty()) {
        return current;
    }
    bundle = QFileInfo(bundle).canonicalFilePath();
    if (bundle.isEmpty()) {
        return current;
    }
    QStringList kept;
    const QStringList entries = current.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        if (entry.startsWith(bundle + QLatin1Char('/'))) {
            continue;
        }
        kept.append(entry);
    }
    return kept.join(QLatin1Char(':'));
}
} // namespace

RpcError::RpcError(const QString &message, int code)
    : std::runtime_error(message.toStdString())
    , m_message(message)
    , m_code(code)
{
}

RpcError::RpcError(const RpcError &other)
    : std::runtime_error(other)
    , m_message(other.m_message)
    , m_code(other.m_code)
{
}

QString RpcError::message() const
{
    return m_message;
}

int RpcError::code() const
{
    return m_code;
}

Wire::Wire(const QStringList &command, const QString &cwd, const QHash<QString, QString> &env, RequestHandler onRequest,
        NotificationHandler onNotification, ExitHandler onExit)
    : m_onRequest(std::move(onRequest))
    , m_onNotification(std::move(onNotification))
    , m_onExit(std::move(onExit))
{
    m_proc = new QProcess(this);
    if (!cwd.isEmpty()) {
        m_proc->setWorkingDirectory(cwd);
    }
    QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();
    // Run from an AppImage the child must not load the bundle's libraries; see
    // sanitizedLibraryPath(). A caller-provided value still wins.
    pe.insert(QStringLiteral("LD_LIBRARY_PATH"), sanitizedLibraryPath());
    for (auto it = env.begin(); it != env.end(); ++it) {
        pe.insert(it.key(), it.value());
    }
    m_proc->setProcessEnvironment(pe);
    m_proc->setProgram(command.value(0));
    m_proc->setArguments(command.mid(1));
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
}

Wire::~Wire()
{
    if (m_proc != nullptr && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(500);
    }
}

void Wire::start()
{
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &Wire::onReadyReadStdout);
    connect(m_proc, &QProcess::readyReadStandardError, this, &Wire::onReadyReadStderr);
    connect(m_proc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        Q_UNUSED(status)
        onProcessFinished(exitCode);
    });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            QMutexLocker lock(&m_startMutex);
            if (!m_startDone) {
                m_startError = QStringLiteral("could not start %1: %2").arg(m_proc->program(), m_proc->errorString());
                m_startDone = true;
                m_startedCondition.wakeAll();
            }
        }
    });
    m_proc->start();
    if (!m_proc->waitForStarted(5000)) {
        QMutexLocker lock(&m_startMutex);
        m_startError = QStringLiteral("could not start %1: %2").arg(m_proc->program(), m_proc->errorString());
        m_startDone = true;
        m_startedCondition.wakeAll();
        return;
    }
    QMutexLocker lock(&m_startMutex);
    m_startDone = true;
    m_startedCondition.wakeAll();
}

bool Wire::waitStarted(int timeoutMs, QString *error)
{
    QMutexLocker lock(&m_startMutex);
    if (!m_startDone) {
        m_startedCondition.wait(&m_startMutex, static_cast<unsigned long>(timeoutMs));
    }
    if (!m_startError.isEmpty()) {
        if (error != nullptr) {
            *error = m_startError;
        }
        return false;
    }
    return true;
}

bool Wire::alive() const
{
    QMutexLocker lock(&m_mutex);
    if (m_exitCode != -1) {
        return false;
    }
    return m_proc->state() != QProcess::NotRunning;
}

bool Wire::writing() const
{
    return m_writing.load();
}

int Wire::exitCode() const
{
    QMutexLocker lock(&m_mutex);
    return m_exitCode;
}

void Wire::onWorker(const std::function<void()> &fn)
{
    if (QThread::currentThread() == thread()) {
        fn();
        return;
    }
    QMetaObject::invokeMethod(this, fn, Qt::QueuedConnection);
}

void Wire::sendRequest(const QString &method, const QJsonObject &params, Reply reply)
{
    qint64 rid = 0;
    bool dead = false;
    {
        QMutexLocker lock(&m_mutex);
        rid = m_nextId++;
        if (m_exitCode != -1) {
            dead = true;
        } else {
            m_pending.insert(rid, reply);
        }
    }
    if (dead) {
        reply(QJsonValue(),
                QJsonObject {
                        { QStringLiteral("code"), INTERNAL_ERROR }, { QStringLiteral("message"), exitMessage() } });
        return;
    }
    QJsonObject msg { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") }, { QStringLiteral("id"), rid },
        { QStringLiteral("method"), method }, { QStringLiteral("params"), params } };
    const QByteArray line = QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n';
    if (line.size() > MAX_LINE) {
        {
            QMutexLocker lock(&m_mutex);
            m_pending.remove(rid);
        }
        const QString refused = QStringLiteral("the request is %1 MiB and orbit reads a line of up to %2 MiB")
                                        .arg(line.size() / (1024 * 1024))
                                        .arg(MAX_LINE / (1024 * 1024));
        reply(QJsonValue(),
                QJsonObject { { QStringLiteral("code"), INTERNAL_ERROR }, { QStringLiteral("message"), refused } });
        return;
    }
    sendLine(line);
}

QJsonValue Wire::call(const QString &method, const QJsonObject &params, int timeoutMs)
{
    struct Box {
        QMutex mutex;
        QWaitCondition cond;
        QJsonValue result;
        QJsonObject error;
        bool done = false;
    };
    // The reply may arrive after this returns (a late answer to a timed-out
    // call), so the wait state is heap-allocated and the lambda keeps it alive.
    auto box = std::make_shared<Box>();
    sendRequest(method, params, [box](const QJsonValue &result, const QJsonObject &error) {
        QMutexLocker lock(&box->mutex);
        box->result = result;
        box->error = error;
        box->done = true;
        box->cond.wakeAll();
    });
    {
        QMutexLocker lock(&box->mutex);
        if (!box->done) {
            box->cond.wait(&box->mutex, static_cast<unsigned long>(timeoutMs));
        }
        if (!box->done) {
            throw RpcError(QStringLiteral("no reply to %1 within %2s").arg(method).arg(timeoutMs / 1000.0, 0, 'g'));
        }
        if (!box->error.isEmpty()) {
            throw RpcError(box->error.value(QStringLiteral("message")).toString(),
                    box->error.value(QStringLiteral("code")).toInt());
        }
        return box->result;
    }
}

void Wire::notify(const QString &method, const QJsonObject &params)
{
    sendObject(QJsonObject { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") }, { QStringLiteral("method"), method },
            { QStringLiteral("params"), params } });
}

void Wire::respond(const QJsonValue &id, const QJsonValue &result)
{
    sendObject(QJsonObject { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") }, { QStringLiteral("id"), id },
            { QStringLiteral("result"), result } });
}

void Wire::fail(const QJsonValue &id, int code, const QString &message)
{
    sendObject(QJsonObject { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") }, { QStringLiteral("id"), id },
            { QStringLiteral("error"),
                    QJsonObject { { QStringLiteral("code"), code }, { QStringLiteral("message"), message } } } });
}

void Wire::sendObject(const QJsonObject &msg)
{
    sendLine(QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n');
}

void Wire::sendLine(const QByteArray &data)
{
    onWorker([this, data]() { writeDirect(data); });
}

void Wire::writeDirect(const QByteArray &data)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_exitCode != -1 || m_proc->state() == QProcess::NotRunning) {
            return;
        }
    }
    m_writing.store(true);
    m_proc->write(data);
    m_proc->waitForBytesWritten(50);
    m_writing.store(false);
}

void Wire::onReadyReadStdout()
{
    // Lines are framed by newlines; keep whatever is short of one.
    static thread_local QByteArray buffer;
    buffer.append(m_proc->readAllStandardOutput());
    auto note = [this](const QString &line) {
        m_stderrTail.push_back(line);
        while (static_cast<int>(m_stderrTail.size()) > STDERR_LINES) {
            m_stderrTail.pop_front();
        }
    };
    int start = 0;
    int nl = -1;
    while ((nl = buffer.indexOf('\n', start)) != -1) {
        const QByteArray line = buffer.mid(start, nl - start).trimmed();
        start = nl + 1;
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError parseError {};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            // Not the protocol; kept so a crash report shows it.
            note(QStringLiteral("stdout: ") + QString::fromUtf8(line.left(200)));
            continue;
        }
        if (!doc.isObject()) {
            // JSON that is not a message (a bare value, an array) is ignored.
            continue;
        }
        {
            QMutexLocker lock(&m_mutex);
            if (m_gone) {
                continue;
            }
        }
        try {
            dispatch(doc.object());
        } catch (const std::exception &e) {
            // One message the agent got wrong must not end the reading.
            note(QStringLiteral("skipped: ") + QString::fromUtf8(e.what()));
        }
    }
    buffer.remove(0, start);
}

void Wire::onReadyReadStderr()
{
    static thread_local QByteArray buffer;
    buffer.append(m_proc->readAllStandardError());
    int start = 0;
    int nl = -1;
    while ((nl = buffer.indexOf('\n', start)) != -1) {
        const QString line = QString::fromUtf8(buffer.mid(start, nl - start)).trimmed();
        start = nl + 1;
        m_stderrTail.push_back(line);
        while (static_cast<int>(m_stderrTail.size()) > STDERR_LINES) {
            m_stderrTail.pop_front();
        }
    }
    buffer.remove(0, start);
}

void Wire::dispatch(const QJsonObject &msg)
{
    const QJsonValue method = msg.value(QStringLiteral("method"));
    QJsonObject params = msg.value(QStringLiteral("params")).toObject();
    if (!method.isUndefined() && !method.isNull() && msg.contains(QStringLiteral("id"))) {
        if (m_onRequest) {
            m_onRequest(msg.value(QStringLiteral("id")), method.toString(), params);
        }
    } else if (!method.isUndefined() && !method.isNull()) {
        if (m_onNotification) {
            m_onNotification(method.toString(), params);
        }
    } else if (msg.contains(QStringLiteral("id"))) {
        Reply reply;
        {
            QMutexLocker lock(&m_mutex);
            reply = m_pending.take(msg.value(QStringLiteral("id")).toInt());
        }
        if (reply) {
            QJsonObject error;
            const QJsonValue err = msg.value(QStringLiteral("error"));
            if (!err.isUndefined() && !err.isNull()) {
                if (err.isObject()) {
                    error = err.toObject();
                    if (error.value(QStringLiteral("message")).toString().isEmpty()) {
                        error.insert(QStringLiteral("message"),
                                QString::fromUtf8(QJsonDocument(error).toJson(QJsonDocument::Compact)));
                    }
                } else {
                    error = QJsonObject { { QStringLiteral("code"), 0 },
                        { QStringLiteral("message"), err.toString() } };
                }
            }
            reply(msg.value(QStringLiteral("result")), error);
        }
    }
}

void Wire::onProcessFinished(int code)
{
    // Drain whatever the child wrote before it exited.
    onReadyReadStdout();
    onReadyReadStderr();
    goneNow();
    Q_UNUSED(code)
}

void Wire::goneNow()
{
    int code = -1;
    QList<Reply> pending;
    {
        QMutexLocker lock(&m_mutex);
        if (m_gone) {
            return;
        }
        m_gone = true;
        code = m_proc->exitCode();
        m_exitCode = code;
        pending = m_pending.values();
        m_pending.clear();
    }
    const QJsonObject error { { QStringLiteral("code"), INTERNAL_ERROR },
        { QStringLiteral("message"), exitMessage() } };
    for (const Reply &reply : pending) {
        reply(QJsonValue(), error);
    }
    if (m_onExit) {
        m_onExit(code);
    }
    Q_EMIT exitReported(code);
}

void Wire::close(int graceMs)
{
    std::function<void()> doClose = [this, graceMs]() {
        {
            QMutexLocker lock(&m_mutex);
            if (m_gone || m_proc->state() == QProcess::NotRunning) {
                return;
            }
        }
        m_proc->closeWriteChannel();
        if (!m_proc->waitForFinished(graceMs)) {
            m_proc->kill();
            m_proc->waitForFinished(1000);
        }
        goneNow();
    };
    if (QThread::currentThread() == thread()) {
        doClose();
    } else {
        QMetaObject::invokeMethod(this, doClose, Qt::BlockingQueuedConnection);
    }
}

QString Wire::exitMessage() const
{
    int code = -1;
    {
        QMutexLocker lock(&m_mutex);
        code = m_exitCode != -1 ? m_exitCode : (m_proc != nullptr ? m_proc->exitCode() : -1);
    }
    QStringList tail;
    for (const QString &l : m_stderrTail) {
        if (!l.isEmpty()) {
            tail.append(l);
        }
    }
    const QString head = QStringLiteral("the agent exited (code %1)").arg(code);
    return tail.isEmpty() ? head : head + QStringLiteral(":\n") + tail.join(QLatin1Char('\n'));
}

QStringList Wire::stderrTail() const
{
    QStringList out;
    for (const QString &l : m_stderrTail) {
        out.append(l);
    }
    return out;
}

} // namespace tqtw_orbitacp
