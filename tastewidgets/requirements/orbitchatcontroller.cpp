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

#include "orbitchatcontroller.h"

#include "tqtw_conversation.h"

#include <QMutexLocker>
#include <QThread>
#include <QWaitCondition>
#include <memory>

namespace requirement {

/*!
 * The state shared between the worker and the GUI thread while a permission
 * request or a form waits to be answered.
 */
struct OrbitChatController::DialogState {
    QMutex mutex;
    QWaitCondition cond;
    bool answered = false;
    QString permission;
    QList<tqtw_orbitacp::Answer> answers;
};

namespace {

//: The question handler the conversation calls on one of its worker threads;
//: it hands the questions to the GUI and waits for the answer.
class Questions : public tqtw_orbitacp::QuestionHandler
{
public:
    explicit Questions(OrbitChatController *self)
        : m_self(self)
    {
    }

    QList<tqtw_orbitacp::Answer> ask(const QList<tqtw_orbitacp::Question> &questions) override
    {
        return m_self->runQuestions(questions);
    }

private:
    OrbitChatController *m_self;
};

} // namespace

OrbitChatController::OrbitChatController(QObject *parent)
    : QObject(parent)
    , m_dialog(std::make_shared<DialogState>())
    , m_questionHandler(std::make_shared<Questions>(this))
{
    qRegisterMetaType<tqtw_orbitacp::Event>();
    qRegisterMetaType<tqtw_orbitacp::PermissionRequest>();
    qRegisterMetaType<QList<tqtw_orbitacp::Question>>();
}

OrbitChatController::~OrbitChatController()
{
    shutdown();
}

bool OrbitChatController::busy() const
{
    QMutexLocker locker(&m_mutex);
    return m_busy;
}

void OrbitChatController::start(const QString &cwd, const QString &briefing)
{
    m_cwd = cwd;
    m_briefing = briefing;
    m_worker = QThread::create([this]() { onWorkerStarted(); });
    m_worker->setObjectName(QStringLiteral("orbit-chat"));
    m_worker->start();
}

void OrbitChatController::onWorkerStarted()
{
    tqtw_orbitacp::Conversation::Options options;
    options.cwd = m_cwd;
    options.briefing = m_briefing;
    options.questions = m_questionHandler.get();
    std::unique_ptr<tqtw_orbitacp::Conversation> conversation;
    try {
        conversation = std::make_unique<tqtw_orbitacp::Conversation>(options);
        conversation->permissions = [this](const tqtw_orbitacp::PermissionRequest &request) { return runRule(request); };
    } catch (const std::exception &e) {
        Q_EMIT failed(QString::fromUtf8(e.what()));
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_conversation = conversation.get();
        m_started = true;
    }
    Q_EMIT ready();

    // Serve questions until shutdown.
    for (;;) {
        QString question;
        {
            QMutexLocker locker(&m_mutex);
            while (!m_shuttingDown && !m_hasQuestion) {
                m_cond.wait(&m_mutex);
            }
            if (m_shuttingDown) {
                break;
            }
            question = m_pendingQuestion;
            m_pendingQuestion.clear();
            m_hasQuestion = false;
            m_busy = true;
        }
        runQuestion(conversation.get(), question);
        {
            QMutexLocker locker(&m_mutex);
            m_busy = false;
        }
    }
    conversation->close();
    {
        QMutexLocker locker(&m_mutex);
        m_conversation = nullptr;
    }
}

void OrbitChatController::runQuestion(tqtw_orbitacp::Conversation *conversation, const QString &text)
{
    try {
        conversation->beginAsk(text);
        tqtw_orbitacp::Event event;
        while (conversation->nextEvent(event)) {
            Q_EMIT eventReceived(event);
        }
    } catch (const std::exception &e) {
        Q_EMIT failed(QString::fromUtf8(e.what()));
    }
    Q_EMIT turnDone(conversation->stoppedBecause());
}

void OrbitChatController::ask(const QString &text)
{
    {
        QMutexLocker locker(&m_mutex);
        if (!m_started || m_shuttingDown || m_busy || m_hasQuestion) {
            return;
        }
        m_pendingQuestion = text;
        m_hasQuestion = true;
    }
    m_cond.wakeAll();
}

void OrbitChatController::cancel()
{
    tqtw_orbitacp::Conversation *conversation = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        conversation = m_conversation;
    }
    if (conversation != nullptr) {
        conversation->cancel();
    }
    // Wake a waiting dialog so the worker is not stuck behind it either.
    {
        QMutexLocker locker(&m_dialog->mutex);
        m_dialog->answered = true;
        m_dialog->permission = QString::fromLatin1(tqtw_orbitacp::REJECT);
        m_dialog->cond.wakeAll();
    }
}

void OrbitChatController::answerPermission(const QString &optionId)
{
    QMutexLocker locker(&m_dialog->mutex);
    m_dialog->permission = optionId;
    m_dialog->answered = true;
    m_dialog->cond.wakeAll();
}

void OrbitChatController::answerQuestions(const QList<tqtw_orbitacp::Answer> &answers)
{
    QMutexLocker locker(&m_dialog->mutex);
    m_dialog->answers = answers;
    m_dialog->answered = true;
    m_dialog->cond.wakeAll();
}

QString OrbitChatController::runRule(const tqtw_orbitacp::PermissionRequest &request)
{
    QMutexLocker locker(&m_dialog->mutex);
    m_dialog->answered = false;
    m_dialog->permission = QString::fromLatin1(tqtw_orbitacp::REJECT);
    locker.unlock();
    Q_EMIT permissionRequested(request);
    locker.relock();
    while (!m_dialog->answered) {
        m_dialog->cond.wait(&m_dialog->mutex);
    }
    return m_dialog->permission;
}

QList<tqtw_orbitacp::Answer> OrbitChatController::runQuestions(const QList<tqtw_orbitacp::Question> &questions)
{
    QMutexLocker locker(&m_dialog->mutex);
    m_dialog->answered = false;
    m_dialog->answers.clear();
    locker.unlock();
    Q_EMIT questionsRequested(questions);
    locker.relock();
    while (!m_dialog->answered) {
        m_dialog->cond.wait(&m_dialog->mutex);
    }
    return m_dialog->answers;
}

void OrbitChatController::shutdown()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_shuttingDown) {
            return;
        }
        m_shuttingDown = true;
    }
    m_cond.wakeAll();
    {
        QMutexLocker locker(&m_dialog->mutex);
        m_dialog->answered = true;
        m_dialog->cond.wakeAll();
    }
    if (m_worker != nullptr) {
        m_worker->wait(10000);
        delete m_worker;
        m_worker = nullptr;
    }
}

} // namespace requirement
