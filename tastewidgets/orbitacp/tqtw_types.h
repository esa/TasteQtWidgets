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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <stdexcept>

/*!
   A C++ port of the orbit-acp reference client (the Python package
   `orbit_acp`). It speaks the Agent Client Protocol (v1) to the `orbit`
   process so an application can ask questions, stream the events of an
   answer, answer the agent's permission requests, hold the files and
   terminals the agent works with, and call orbit's own methods.

   The contract implemented here is the one written down in the orbit-acp
   specification: the same calls, names, defaults and error messages as the
   Python reference. The Python reference is the source of truth; when the
   two disagree, this port is wrong.
 */
namespace tqtw_orbitacp {

//: The environment variable naming an agent other than the `orbit` on the PATH.
extern const char *const AGENT_ENV;

//: The answers a permission rule may give, as the option ids orbit offers.
extern const char *const ONCE;
extern const char *const ALWAYS;
extern const char *const REJECT;

//: The protocol version this client speaks.
constexpr int PROTOCOL_VERSION = 1;

//: How long a setup call (initialize, the session methods) may take.
constexpr int SETUP_TIMEOUT_MS = 120 * 1000;

//: How long a cancelled question waits for the agent to confirm it stopped.
constexpr int CONFIRM_WAIT_MS = 30 * 1000;

/*!
 * Anything that went wrong on the way to or from orbit, said plainly.
 */
class OrbitError : public std::runtime_error
{
public:
    explicit OrbitError(const QString &message);
    OrbitError(const OrbitError &other);
    ~OrbitError() override;

    QString message() const;

private:
    QString m_message;
};

/*!
 * No `orbit` on the PATH and nothing named in ORBIT_ACP_AGENT.
 */
class AgentNotFound : public OrbitError
{
public:
    explicit AgentNotFound(const QString &message);
    AgentNotFound(const AgentNotFound &other);
    ~AgentNotFound() override;
};

// ---- simple records --------------------------------------------------------

//: One thing a select config option takes.
struct Choice {
    QString value;
    QString name;
};

//: A setting of the session the agent lets an app change.
struct ConfigOption {
    QString id;
    QString name;
    QString category;
    QString type = QStringLiteral("select");
    QJsonValue value;
    QList<Choice> choices;
};

//: A slash command the agent takes.
struct Command {
    QString name;
    QString description;
    QString hint;
};

//: One orbit agent.
struct AgentInfo {
    QString id;
    QString name;
    QString description;
    QString mode;
};

//: One model orbit can use.
struct ModelInfo {
    QString id;
    QString name;
    QString provider;
    QString arg;
    bool vision = false;
    int contextLimit = 0;
    int outputLimit = 0;
    bool current = false;
};

//: A skill orbit can load into the next question.
struct Skill {
    QString name;
    QString description;
};

//: A workflow orbit can run by name.
struct Workflow {
    QString name;
    QString description;
    QJsonObject params;
    bool disabled = false;
    QStringList problems;
};

//: An agent working under the conversation.
struct RunningAgent {
    QString id;
    QString title;
    QString agent;
    QString status;
    int depth = 0;
    QString session;
    QString parent;
};

//: A stored conversation, as Conversation::list reports it.
struct ConversationInfo {
    QString id;
    QString title;
    QString cwd;
    QString updated;
};

//: Text the model should read with the question.
struct Resource {
    QString uri;
    QString text;
    QString mime;
};

//: One edit the agent made.
struct Diff {
    QString path;
    QString oldText;
    QString newText;
};

//: One line of the agent's plan.
struct PlanStep {
    QString title;
    QString status;
    QString priority;
};

//: One answer the agent accepts to a permission request.
struct Option {
    QString id;
    QString name;
    QString kind;
};

//: What a terminal has shown so far.
struct TerminalOutput {
    QString text;
    bool truncated = false;
    int exitCode = -1;
    QString signal;
};

//: How a command ended.
struct TerminalExit {
    int exitCode = -1;
    QString signal;
};

//: One thing the person may pick.
struct QuestionChoice {
    QString text;
    QString note;
};

//: A question the model stopped to ask.
struct Question {
    QString text;
    QString topic;
    QList<QuestionChoice> choices;
    bool many = false;
    bool freeText = true;
};

//: What the person gave for one question.
struct Answer {
    QStringList picked;
    QString typed;
};

/*!
 * The agent wants to do something and asks. A rule returns the id of one of
 * `options` (or one of ONCE/ALWAYS/REJECT).
 */
struct PermissionRequest {
    QString title;
    QString toolKind = QStringLiteral("other");
    QString permission;
    QStringList patterns;
    QList<Option> options;
    QString toolId;
    QJsonObject raw;

    //: orbit's permission names whose first pattern is a shell command line.
    static const QStringList SHELL_PERMISSIONS;

    //: The first pattern when `permission` is a shell permission, else empty.
    QString command() const;

    //: The option id `answer` names, by id or by kind; REJECT when it names none.
    QString optionId(const QString &answer) const;

    static PermissionRequest fromWire(const QJsonObject &params);
};

/*!
 * One event of a running question. `kind` is the plain word for the kind of
 * event; `raw` is the update as it came over the wire.
 */
struct Event {
    enum Kind
    {
        Text,
        Thought,
        UserText,
        Tool,
        Task,
        Plan,
        Usage,
        Raw,
    };

    Kind kind = Raw;
    //: For Raw, the name on the wire.
    QString wireKind;

    //: Text/Thought/UserText.
    QString text;

    //: Tool/Task.
    QString id;
    QString title;
    QString toolKind;
    QString status;
    QJsonObject input;
    QString output;
    bool hasDiff = false;
    Diff diff;
    QString terminal;
    QStringList files;

    //: Task.
    QString agent;
    int depth = 0;
    QString session;
    QString parent;

    //: Plan.
    QList<PlanStep> steps;

    //: Usage.
    int used = 0;
    int size = 0;

    QJsonObject raw;

    bool done() const { return kind == Tool && status == QStringLiteral("completed"); }
    bool failed() const { return kind == Tool && status == QStringLiteral("failed"); }
    bool running() const
    {
        return kind == Tool && (status == QStringLiteral("pending") || status == QStringLiteral("in_progress"));
    }
};

// ---- the client's side: the app's handlers ---------------------------------

/*!
 * The files the app holds (an editor's buffers). `line` is 1-based, and -1
 * when the agent did not send one. `read` must fail (throw) for a path the
 * app does not hold, so orbit falls back to the disk.
 */
struct FileHandler {
    virtual ~FileHandler();
    virtual QString read(const QString &path, int line, int limit) = 0;
    virtual void write(const QString &path, const QString &content) = 0;
};

//: The terminals the app holds.
struct TerminalHandler {
    virtual ~TerminalHandler();
    virtual QString create(const QString &command, const QStringList &args, const QString &cwd,
            const QHash<QString, QString> &env, int outputLimit) = 0;
    virtual TerminalOutput output(const QString &id) = 0;
    virtual TerminalExit wait(const QString &id) = 0;
    virtual void kill(const QString &id) = 0;
    virtual void release(const QString &id) = 0;
};

//: What can put the model's questions to the person.
struct QuestionHandler {
    virtual ~QuestionHandler();
    //: One answer per question, in order; an empty list declines the form.
    virtual QList<Answer> ask(const QList<Question> &questions) = 0;
};

// ---- parsing the wire ------------------------------------------------------

/*!
 * The event one `session/update` payload (the `update` object) is.
 */
Event eventFrom(const QJsonObject &update);

//: The `ConfigOption`s a `configOptions` list holds.
QList<ConfigOption> optionsFrom(const QJsonArray &rows);

//: One question a form asks, with the field key it is read from.
struct FormQuestion {
    Question question;
    QString key;
};

/*!
 * The questions a form asks, from its `requestedSchema`: one per field in
 * order, and a field named `<key>_text` marks the question before it as
 * taking a typed answer.
 */
QList<FormQuestion> formQuestions(const QJsonObject &schema);

} // namespace tqtw_orbitacp

Q_DECLARE_METATYPE(tqtw_orbitacp::Event)
Q_DECLARE_METATYPE(tqtw_orbitacp::PermissionRequest)
Q_DECLARE_METATYPE(tqtw_orbitacp::Question)
Q_DECLARE_METATYPE(tqtw_orbitacp::Answer)
