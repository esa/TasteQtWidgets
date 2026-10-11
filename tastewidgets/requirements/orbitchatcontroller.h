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

#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QWaitCondition>
#include <memory>

class QThread;

namespace tqtw_orbitacp {
class Conversation;
}

namespace requirement {

/*!
 * The chatbot's engine: it owns a `Conversation` on a worker thread of its own
 * and turns the blocking client into Qt signals for the GUI.

 * The worker thread owns the conversation and runs `beginAsk`/`nextEvent` in a
 * loop. A permission request or a form pauses that worker on a condition and
 * wakes the GUI through a signal; the dialog answers and the worker goes on.
 * `cancel` and `shutdown` may be called from the GUI thread at any time.
 */
class OrbitChatController : public QObject
{
    Q_OBJECT

public:
    explicit OrbitChatController(QObject *parent = nullptr);
    ~OrbitChatController() override;

    //: Start the agent. `cwd` is the project the agent works in.
    void start(const QString &cwd, const QString &briefing = QString());
    //: Ask a question. Ignored while one runs or before the agent is ready.
    void ask(const QString &text);
    //: Stop the question being answered, or release a waiting dialog.
    void cancel();
    //: Shut the conversation down and stop the worker.
    void shutdown();

    bool busy() const;

    //: Called from the GUI thread to answer a permission request.
    void answerPermission(const QString &optionId);
    //: Called from the GUI thread to answer a form (empty list declines).
    void answerQuestions(const QList<tqtw_orbitacp::Answer> &answers);

    //: The conversation's question handler calls this on a worker thread.
    QList<tqtw_orbitacp::Answer> runQuestions(const QList<tqtw_orbitacp::Question> &questions);

Q_SIGNALS:
    void ready();
    void failed(const QString &message);
    void eventReceived(const tqtw_orbitacp::Event &event);
    void turnDone(const QString &stoppedBecause);
    void permissionRequested(const tqtw_orbitacp::PermissionRequest &request);
    void questionsRequested(const QList<tqtw_orbitacp::Question> &questions);

private:
    struct DialogState;

    void onWorkerStarted();
    void runQuestion(tqtw_orbitacp::Conversation *conversation, const QString &text);
    QString runRule(const tqtw_orbitacp::PermissionRequest &request);

    QThread *m_worker = nullptr;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    tqtw_orbitacp::Conversation *m_conversation = nullptr;
    QString m_pendingQuestion;
    bool m_hasQuestion = false;
    bool m_busy = false;
    bool m_started = false;
    bool m_shuttingDown = false;
    QString m_cwd;
    QString m_briefing;

    std::shared_ptr<DialogState> m_dialog;
    std::shared_ptr<tqtw_orbitacp::QuestionHandler> m_questionHandler;
};

} // namespace requirement
