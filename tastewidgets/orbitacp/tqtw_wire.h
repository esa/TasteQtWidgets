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

#pragma once

#include "tqtw_types.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QWaitCondition>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>

class QProcess;

namespace tqtw_orbitacp {

//: A reply that is an error, or a request that could not be delivered.
class RpcError : public std::runtime_error
{
public:
    RpcError(const QString &message, int code = 0);
    RpcError(const RpcError &other);

    QString message() const;
    int code() const;

private:
    QString m_message;
    int m_code;
};

/*!
 * One agent process and the JSON-RPC messages on its pipes.

 * This is the transport under `Conversation` and nothing more. It lives in a
 * worker thread of its own: `start()` creates the child QProcess there and the
 * readyRead/finished signals are delivered there, so callbacks run on the
 * worker thread. Any thread may send; the writes are queued to the worker so
 * one message is never interleaved with another. `call` blocks the calling
 * thread (driving the reply through the worker) until the answer arrives.
 */
class Wire : public QObject
{
    Q_OBJECT

public:
    using Reply = std::function<void(const QJsonValue &result, const QJsonObject &error)>;
    using RequestHandler = std::function<void(const QJsonValue &id, const QString &method, const QJsonObject &params)>;
    using NotificationHandler = std::function<void(const QString &method, const QJsonObject &params)>;
    using ExitHandler = std::function<void(int code)>;

    Wire(const QStringList &command, const QString &cwd, const QHash<QString, QString> &env, RequestHandler onRequest,
            NotificationHandler onNotification, ExitHandler onExit);
    ~Wire() override;

    //: Create the child process. Runs on the worker thread.
    void start();
    //: Wait until `start` has run; false (with `error` set) when it failed.
    bool waitStarted(int timeoutMs, QString *error);

    bool alive() const;
    //: True while a thread is inside a write.
    bool writing() const;
    int exitCode() const;

    //: Send a request; `reply(result, error)` runs when the answer arrives.
    void sendRequest(const QString &method, const QJsonObject &params, Reply reply);
    //: Send a request and wait for its result; throws RpcError on error or timeout.
    QJsonValue call(const QString &method, const QJsonObject &params, int timeoutMs);

    void notify(const QString &method, const QJsonObject &params);
    void respond(const QJsonValue &id, const QJsonValue &result);
    void fail(const QJsonValue &id, int code, const QString &message);

    //: Hang up: close the agent's stdin and kill it if it has not left.
    void close(int graceMs = 5000);

    QString exitMessage() const;
    QStringList stderrTail() const;

    //: JSON-RPC codes the wire uses when it declines something itself.
    static constexpr int METHOD_NOT_FOUND = -32601;
    static constexpr int INVALID_PARAMS = -32602;
    static constexpr int INTERNAL_ERROR = -32603;

Q_SIGNALS:
    //: Emitted on the worker thread, once, when the agent is gone.
    void exitReported(int code);

private:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode);
    void dispatch(const QJsonObject &msg);
    void goneNow();
    void sendObject(const QJsonObject &msg);
    void sendLine(const QByteArray &data);
    void writeDirect(const QByteArray &data);
    //: Run `fn` on the worker thread (immediately when already there).
    void onWorker(const std::function<void()> &fn);

    QProcess *m_proc = nullptr;
    RequestHandler m_onRequest;
    NotificationHandler m_onNotification;
    ExitHandler m_onExit;

    //: Guards pending, nextId, gone, exitCode, writing.
    mutable QMutex m_mutex;
    QHash<qint64, Reply> m_pending; //: request id (int) -> reply
    qint64 m_nextId = 1;
    bool m_gone = false;
    int m_exitCode = -1;
    std::atomic<bool> m_writing { false };

    //: start() handshake.
    QMutex m_startMutex;
    QWaitCondition m_startedCondition;
    bool m_startDone = false;
    QString m_startError;

    std::deque<QString> m_stderrTail;
};

} // namespace tqtw_orbitacp
