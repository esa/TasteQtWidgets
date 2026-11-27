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
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QWaitCondition>
#include <functional>

class QThread;

namespace tqtw_orbitacp {

class Wire;

//: The rule that answers a permission request: it returns one of ONCE,
//: ALWAYS, REJECT, or an option id/kind from the request.
using Rule = std::function<QString(const PermissionRequest &)>;

//: A rule that lets the agent do everything it asks.
Rule allowAll();
//: A rule that refuses everything. The default.
Rule denyAll();

//: An image the model should look at: a file path, or bytes with a MIME type.
struct Image {
    QString path;
    QByteArray data;
    QString mime;
};

/*!
 * The command that starts the agent, or an empty list when there is none.
 */
QStringList agentCommand();

//: True when an agent can be started on this machine.
bool available();

/*!
 * One conversation with orbit.

 * `cwd` is the project the agent works in (the current directory when not
 * given). `agent` picks the orbit agent by name. `briefing` is text sent ahead
 * of the first question only. `permissions` is the rule that answers what the
 * agent asks to do; without one, everything is refused.

 * The agent's output is read on a worker thread of the library's own, but every
 * event and every call of the permission rule happens on the thread that is
 * iterating `nextEvent` (or sitting in `ask`). `cancel` and `close` are safe
 * from any thread.
 */
class Conversation : public QObject
{
    Q_OBJECT

public:
    struct Options {
        QString cwd;
        QString agent;
        QString briefing;
        QHash<QString, QString> env;
        QStringList command;
        int timeoutMs = SETUP_TIMEOUT_MS;
        QString session;
        bool history = false;
        FileHandler *files = nullptr;
        TerminalHandler *terminals = nullptr;
        QuestionHandler *questions = nullptr;
    };

    using EventCallback = std::function<void(const Event &)>;

    explicit Conversation(QObject *parent = nullptr);
    explicit Conversation(const Options &options, QObject *parent = nullptr);
    ~Conversation() override;

    //: The stored conversations, newest first; those of `cwd` when given.
    static QList<ConversationInfo> list(const QString &cwd = QString(), const QHash<QString, QString> &env = {},
            const QStringList &command = {}, int timeoutMs = SETUP_TIMEOUT_MS);
    //: Remove the stored conversation `session`.
    static void remove(const QString &session, const QString &cwd = QString(), const QHash<QString, QString> &env = {},
            const QStringList &command = {}, int timeoutMs = SETUP_TIMEOUT_MS);

    // ---- what the conversation knows -------------------------------------
    QString id() const { return m_id; }
    QString cwd() const { return m_cwd; }
    QString agent() const { return m_agent; }
    QList<AgentInfo> agents() const { return m_agents; }
    QList<Command> commands() const;
    bool hasCommand(const QString &name) const;
    QList<ConfigOption> options() const { return m_options; }
    QString title() const { return m_title; }
    QString updated() const { return m_updated; }
    QString orbitVersion() const { return m_orbitVersion; }
    QJsonObject capabilities() const { return m_capabilities; }
    QStringList extensions() const { return m_extensions; }
    int exited() const { return m_exited; }
    bool supportsImages() const;
    QString model() const;
    QString lastAnswer() const { return m_lastAnswer; }
    QString stoppedBecause() const { return m_stoppedBecause; }
    bool running() const;
    bool alive() const;
    QList<Event> history() const { return m_history; }

    Rule permissions;
    FileHandler *files = nullptr;
    TerminalHandler *terminals = nullptr;
    QuestionHandler *questions = nullptr;

    void brief(const QString &text) { m_briefing = text; }
    void switchAgent(const QString &name, int timeoutMs = SETUP_TIMEOUT_MS);
    void setOption(const QString &option, const QJsonValue &value);
    void switchModel(const QString &name);

    // ---- asking -----------------------------------------------------------
    /*!
     * Start a question. Events arrive through `nextEvent` until it returns
     * false. Throws OrbitError when a question is already running or the agent
     * is gone.
     */
    void beginAsk(const QString &text, const QList<Image> &images = {}, const QList<Resource> &resources = {},
            const QStringList &resourcePaths = {}, int timeoutMs = -1);

    //: The next event, or false when the question ended. Throws on error.
    bool nextEvent(Event &event);

    //: Ask and wait for the answer; `onEvent` (when given) sees every event.
    QString ask(const QString &text, const EventCallback &onEvent = {}, const QList<Image> &images = {},
            const QList<Resource> &resources = {}, const QStringList &resourcePaths = {}, int timeoutMs = -1);

    //: Stop the question being answered. Safe from any thread.
    void cancel();

    // ---- what orbit lists: its own methods --------------------------------
    QList<AgentInfo> allAgents();
    QList<ModelInfo> models();
    QList<Skill> skills();
    void useSkill(const QString &name, const QString &text = QString());
    QList<Workflow> workflows();
    QJsonObject status();
    QList<RunningAgent> runningAgents();

    // ---- what orbit runs as a turn ----------------------------------------
    QString workflow(const QString &name, const QString &request = QString(), const QString &csv = QString(),
            const QString &agent = QString(), bool header = true, int verify = -1, const QJsonObject &params = {},
            const EventCallback &onEvent = {});
    QString orchestrate(const QString &request, int verify = -1, const EventCallback &onEvent = {});
    QString batch(const QString &csv, const QString &prompt, const QString &agent = QString(), bool header = true,
            bool verify = true, const EventCallback &onEvent = {});
    QString consult(const QString &agent, const QString &question, const EventCallback &onEvent = {});
    QString brainstorm(const QStringList &agents, const QString &question, const EventCallback &onEvent = {});
    QString compact(bool full = false);

    void close();

private:
    struct TurnItem;
    struct Turn;

    void startWire(const QStringList &command, const QHash<QString, QString> &env);
    QJsonValue call(const QString &method, const QJsonObject &params, int timeoutMs);
    QJsonValue ext(const QString &what, const QJsonObject &params = {});

    void onRequest(const QJsonValue &id, const QString &method, const QJsonObject &params);
    void onNotification(const QString &method, const QJsonObject &params);
    void onExit(int code);

    void replied(const QSharedPointer<Turn> &turn, const QJsonValue &result, const QJsonObject &error);
    void enqueueEvent(const QSharedPointer<Turn> &turn, const Event &event);
    void answerPermission(const QJsonValue &id, const PermissionRequest &request);
    void clientCall(const QJsonValue &id, const QString &method, const QJsonObject &params);
    void endTurn(const QSharedPointer<Turn> &turn);
    void timeoutTurn();

    void readHello(const QJsonValue &result);
    void readModes(const QJsonValue &result);
    void readOptions(const QJsonValue &result);
    void refreshVision();
    void waitForCommands();

    // ---- state (guarded by m_mutex) --------------------------------------
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    QWaitCondition m_commandsCond;

    QString m_cwd;
    QString m_id;
    QString m_agent;
    QList<AgentInfo> m_agents;
    QList<Command> m_commands;
    bool m_commandsKnown = false;
    bool m_commandsWaited = false;
    QList<ConfigOption> m_options;
    QString m_title;
    QString m_updated;
    QString m_briefing;
    QString m_orbitVersion;
    QJsonObject m_capabilities;
    QStringList m_extensions;
    int m_exited = -1;
    QList<Event> m_history;
    QString m_lastAnswer;
    QString m_stoppedBecause;
    bool m_cancelled = false;
    bool m_timedOut = false;
    QSharedPointer<Turn> m_turn;
    QSharedPointer<Turn> m_stopping;
    QString m_answerText;

    // ---- worker ----------------------------------------------------------
    QThread *m_worker = nullptr;
    Wire *m_wire = nullptr;
    int m_timeoutMs = SETUP_TIMEOUT_MS;
};

/*!
 * Ask orbit one question and get the answer as text. Starts the agent, asks,
 * and closes it again. Everything the agent asks to do is refused unless
 * `permissions` says otherwise.
 */
QString ask(const QString &text, const QString &cwd = QString(), const QString &agent = QString(),
        Rule permissions = Rule());

} // namespace tqtw_orbitacp
