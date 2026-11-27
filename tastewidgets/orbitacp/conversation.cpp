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

#include "tqtw_conversation.h"

#include "tqtw_wire.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QPointer>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <chrono>
#include <thread>

namespace tqtw_orbitacp {

struct Conversation::TurnItem {
    enum Kind
    {
        KindEvent,
        KindAsk,
        KindCancel,
        KindDone,
        KindError,
        KindDeadline
    };
    Kind kind = KindEvent;
    Event event;
    QJsonValue id;
    PermissionRequest request;
    QJsonValue result;
    QJsonObject error;
};

struct Conversation::Turn {
    QMutex mutex;
    QWaitCondition cond;
    QList<TurnItem> items;
    bool replied = false;
    //: Set once the question is cancelled: when it ends for the caller if the
    //: agent has not confirmed by then.
    bool hasDeadline = false;
    std::chrono::steady_clock::time_point deadline;
};

namespace {

const QString TYPED_FIELD = QStringLiteral("_text");

QString stopReasonWord(const QString &reason)
{
    static const QHash<QString, QString> table {
        { QStringLiteral("end_turn"), QStringLiteral("done") },
        { QStringLiteral("cancelled"), QStringLiteral("cancelled") },
        { QStringLiteral("max_turn_requests"), QStringLiteral("step_limit") },
        { QStringLiteral("max_tokens"), QStringLiteral("max_tokens") },
        { QStringLiteral("refusal"), QStringLiteral("refused") },
    };
    return table.value(reason, reason);
}

QString imageMime(const QString &suffix)
{
    static const QHash<QString, QString> table {
        { QStringLiteral(".png"), QStringLiteral("image/png") },
        { QStringLiteral(".jpg"), QStringLiteral("image/jpeg") },
        { QStringLiteral(".jpeg"), QStringLiteral("image/jpeg") },
        { QStringLiteral(".gif"), QStringLiteral("image/gif") },
        { QStringLiteral(".webp"), QStringLiteral("image/webp") },
    };
    return table.value(suffix.toLower());
}

QString resourceMime(const QString &suffix)
{
    static const QHash<QString, QString> table {
        { QStringLiteral(".md"), QStringLiteral("text/markdown") },
        { QStringLiteral(".txt"), QStringLiteral("text/plain") },
        { QStringLiteral(".json"), QStringLiteral("application/json") },
        { QStringLiteral(".py"), QStringLiteral("text/x-python") },
        { QStringLiteral(".ts"), QStringLiteral("text/typescript") },
        { QStringLiteral(".js"), QStringLiteral("text/javascript") },
        { QStringLiteral(".go"), QStringLiteral("text/x-go") },
        { QStringLiteral(".rs"), QStringLiteral("text/x-rust") },
        { QStringLiteral(".c"), QStringLiteral("text/x-c") },
        { QStringLiteral(".h"), QStringLiteral("text/x-c") },
        { QStringLiteral(".cpp"), QStringLiteral("text/x-c++") },
        { QStringLiteral(".hpp"), QStringLiteral("text/x-c++") },
        { QStringLiteral(".html"), QStringLiteral("text/html") },
        { QStringLiteral(".css"), QStringLiteral("text/css") },
        { QStringLiteral(".yaml"), QStringLiteral("application/yaml") },
        { QStringLiteral(".yml"), QStringLiteral("application/yaml") },
        { QStringLiteral(".toml"), QStringLiteral("application/toml") },
        { QStringLiteral(".xml"), QStringLiteral("application/xml") },
        { QStringLiteral(".csv"), QStringLiteral("text/csv") },
    };
    return table.value(suffix.toLower());
}

//: shlex-quote a value: quoted only when it needs it.
QString shellQuote(const QString &value)
{
    if (!value.contains(QRegularExpression(QStringLiteral("\\s")))) {
        return value;
    }
    QString out = value;
    out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'") + out + QStringLiteral("'");
}

QString workflowLine(const QString &name, const QString &request, const QString &csv, const QString &agent, bool header,
        int verify, const QJsonObject &params)
{
    QStringList parts { QStringLiteral("/workflow"), name };
    if (!csv.isEmpty()) {
        parts << QStringLiteral("--file") << shellQuote(csv);
    }
    if (!agent.isEmpty()) {
        parts << QStringLiteral("--agent") << agent;
    }
    if (!header) {
        parts << QStringLiteral("--no-header");
    }
    if (verify == 0) { // exactly false; -1 is "absent"
        parts << QStringLiteral("--no-verify");
    }
    for (auto it = params.begin(); it != params.end(); ++it) {
        parts << QStringLiteral("--") + it.key() << shellQuote(it.value().toString());
    }
    if (!request.isEmpty()) {
        parts << request;
    }
    return parts.join(QLatin1Char(' '));
}

QString batchLine(const QString &csv, const QString &prompt, const QString &agent, bool header, bool verify)
{
    QStringList parts { QStringLiteral("/batch") };
    if (!agent.isEmpty()) {
        parts << QStringLiteral("--agent") << agent;
    }
    if (!header) {
        parts << QStringLiteral("--no-header");
    }
    if (!verify) {
        parts << QStringLiteral("--no-verify");
    }
    parts << QStringLiteral("--file") << shellQuote(csv) << prompt;
    return parts.join(QLatin1Char(' '));
}

QJsonObject exitStatus(int code, const QString &signal)
{
    if (code < 0 && signal.isEmpty()) {
        return {}; // null while the command runs
    }
    QJsonObject out;
    out.insert(QStringLiteral("exitCode"), code < 0 ? QJsonValue() : QJsonValue(code));
    out.insert(QStringLiteral("signal"), signal.isEmpty() ? QJsonValue() : QJsonValue(signal));
    return out;
}

} // namespace

// ---- module helpers --------------------------------------------------------

QStringList agentCommand()
{
    const QString custom = qEnvironmentVariable(AGENT_ENV);
    if (!custom.isEmpty()) {
        return custom.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    }
    const QString exe = QStandardPaths::findExecutable(QStringLiteral("orbit"));
    if (!exe.isEmpty()) {
        return { exe, QStringLiteral("acp") };
    }
    return {};
}

bool available()
{
    return !agentCommand().isEmpty();
}

Rule allowAll()
{
    return [](const PermissionRequest &) { return QString::fromLatin1(ONCE); };
}

Rule denyAll()
{
    return [](const PermissionRequest &) { return QString::fromLatin1(REJECT); };
}

QString ask(const QString &text, const QString &cwd, const QString &agent, Rule permissions)
{
    Conversation::Options opts;
    opts.cwd = cwd;
    opts.agent = agent;
    Conversation chat(opts);
    if (permissions) {
        chat.permissions = std::move(permissions);
    }
    return chat.ask(text);
}

// ---- one-shot wire helpers -------------------------------------------------

namespace {

//: Start a worker+wire for a one-shot call, run `body`, then close everything.
QJsonValue runOnce(const QStringList &command, const QString &cwd, const QHash<QString, QString> &env, int timeoutMs,
        const std::function<QJsonValue(Wire *)> &body)
{
    Q_UNUSED(timeoutMs)
    if (!cwd.isEmpty() && !QFileInfo(cwd).isDir()) {
        throw OrbitError(QStringLiteral("no such directory '%1'").arg(cwd));
    }
    QStringList cmd = command.isEmpty() ? agentCommand() : command;
    if (cmd.isEmpty()) {
        throw AgentNotFound(QStringLiteral(
                "orbit was not found on the PATH; install it from its checkout (python -m pip install -e .) "
                "or name the agent to run in ORBIT_ACP_AGENT"));
    }
    QThread worker;
    Wire wire(cmd, cwd, env, nullptr, nullptr, nullptr);
    wire.moveToThread(&worker);
    QObject::connect(&worker, &QThread::started, &wire, &Wire::start);
    worker.start();
    QString error;
    if (!wire.waitStarted(6000, &error)) {
        worker.quit();
        worker.wait(6000);
        throw AgentNotFound(error);
    }
    QJsonValue result;
    try {
        result = body(&wire);
    } catch (...) {
        wire.close();
        worker.quit();
        worker.wait(6000);
        throw;
    }
    wire.close();
    worker.quit();
    worker.wait(6000);
    return result;
}

} // namespace

QList<ConversationInfo> Conversation::list(
        const QString &cwd, const QHash<QString, QString> &env, const QStringList &command, int timeoutMs)
{
    QList<ConversationInfo> out;
    const QVector<QJsonObject> objs = [&]() {
        QVector<QJsonObject> v;
        const QJsonValue res = runOnce(command, cwd, env, timeoutMs, [&](Wire *wire) -> QJsonValue {
            wire->call(QStringLiteral("initialize"),
                    QJsonObject { { QStringLiteral("protocolVersion"), PROTOCOL_VERSION },
                            { QStringLiteral("clientCapabilities"), QJsonObject() } },
                    timeoutMs);
            QJsonObject params;
            if (!cwd.isEmpty()) {
                params.insert(QStringLiteral("cwd"), QFileInfo(cwd).absoluteFilePath());
            }
            return wire->call(QStringLiteral("session/list"), params, timeoutMs);
        });
        for (const QJsonValue &s : res.toObject().value(QStringLiteral("sessions")).toArray()) {
            v.append(s.toObject());
        }
        return v;
    }();
    for (const QJsonObject &s : objs) {
        ConversationInfo info;
        info.id = s.value(QStringLiteral("sessionId")).toString();
        info.title = s.value(QStringLiteral("title")).toString();
        info.cwd = s.value(QStringLiteral("cwd")).toString();
        info.updated = s.value(QStringLiteral("updatedAt")).toString();
        out.append(info);
    }
    return out;
}

void Conversation::remove(const QString &session, const QString &cwd, const QHash<QString, QString> &env,
        const QStringList &command, int timeoutMs)
{
    runOnce(command, cwd, env, timeoutMs, [&](Wire *wire) -> QJsonValue {
        wire->call(QStringLiteral("initialize"),
                QJsonObject { { QStringLiteral("protocolVersion"), PROTOCOL_VERSION },
                        { QStringLiteral("clientCapabilities"), QJsonObject() } },
                timeoutMs);
        return wire->call(
                QStringLiteral("session/delete"), QJsonObject { { QStringLiteral("sessionId"), session } }, timeoutMs);
    });
}

// ---- Conversation ----------------------------------------------------------

Conversation::Conversation(QObject *parent)
    : Conversation(Options(), parent)
{
}

Conversation::Conversation(const Options &options, QObject *parent)
    : QObject(parent)
    , permissions(denyAll())
    , files(options.files)
    , terminals(options.terminals)
    , questions(options.questions)
{
    m_cwd = QFileInfo(options.cwd.isEmpty() ? QDir::currentPath() : options.cwd).absoluteFilePath();
    m_timeoutMs = options.timeoutMs;
    m_briefing = options.briefing;
    try {
        startWire(options.command, options.env);
        QJsonObject capabilities;
        capabilities.insert(QStringLiteral("fs"),
                QJsonObject { { QStringLiteral("readTextFile"), files != nullptr },
                        { QStringLiteral("writeTextFile"), files != nullptr } });
        capabilities.insert(QStringLiteral("terminal"), terminals != nullptr);
        if (questions != nullptr) {
            capabilities.insert(
                    QStringLiteral("elicitation"), QJsonObject { { QStringLiteral("form"), QJsonObject() } });
        }
        const QJsonValue hello = call(QStringLiteral("initialize"),
                QJsonObject { { QStringLiteral("protocolVersion"), PROTOCOL_VERSION },
                        { QStringLiteral("clientCapabilities"), capabilities },
                        { QStringLiteral("clientInfo"),
                                QJsonObject { { QStringLiteral("name"), QStringLiteral("orbit-acp-cpp") },
                                        { QStringLiteral("version"), QStringLiteral("1") } } } },
                options.timeoutMs);
        readHello(hello);
        if (!options.session.isEmpty()) {
            const QString method = options.history ? QStringLiteral("session/load") : QStringLiteral("session/resume");
            const QJsonValue res = call(method,
                    QJsonObject { { QStringLiteral("sessionId"), options.session }, { QStringLiteral("cwd"), m_cwd },
                            { QStringLiteral("mcpServers"), QJsonArray() } },
                    options.timeoutMs);
            m_id = options.session;
            readModes(res);
            readOptions(res);
        } else {
            const QJsonValue res = call(QStringLiteral("session/new"),
                    QJsonObject { { QStringLiteral("cwd"), m_cwd }, { QStringLiteral("mcpServers"), QJsonArray() } },
                    options.timeoutMs);
            m_id = res.toObject().value(QStringLiteral("sessionId")).toString();
            if (m_id.isEmpty()) {
                throw OrbitError(QStringLiteral("session/new gave no session id"));
            }
            readModes(res);
            readOptions(res);
        }
        if (!options.agent.isEmpty() && options.agent != m_agent) {
            switchAgent(options.agent, options.timeoutMs);
        }
        refreshVision();
        waitForCommands();
    } catch (...) {
        if (m_wire != nullptr) {
            m_wire->close();
        }
        if (m_worker != nullptr) {
            m_worker->quit();
            m_worker->wait(6000);
        }
        throw;
    }
}

Conversation::~Conversation()
{
    close();
    if (m_worker != nullptr) {
        m_worker->quit();
        m_worker->wait(6000);
        delete m_worker;
        m_worker = nullptr;
    }
    delete m_wire;
    m_wire = nullptr;
}

void Conversation::startWire(const QStringList &command, const QHash<QString, QString> &env)
{
    if (!QFileInfo(m_cwd).isDir()) {
        throw OrbitError(QStringLiteral("no such directory '%1'").arg(m_cwd));
    }
    QStringList cmd = command.isEmpty() ? agentCommand() : command;
    if (cmd.isEmpty()) {
        throw AgentNotFound(QStringLiteral(
                "orbit was not found on the PATH; install it from its checkout (python -m pip install -e .) "
                "or name the agent to run in ORBIT_ACP_AGENT"));
    }
    m_worker = new QThread();
    m_wire = new Wire(
            cmd, m_cwd, env,
            [this](const QJsonValue &id, const QString &method, const QJsonObject &params) {
                onRequest(id, method, params);
            },
            [this](const QString &method, const QJsonObject &params) { onNotification(method, params); },
            [this](int code) { onExit(code); });
    m_wire->moveToThread(m_worker);
    connect(m_worker, &QThread::started, m_wire, &Wire::start);
    m_worker->start();
    QString error;
    if (!m_wire->waitStarted(6000, &error)) {
        throw AgentNotFound(error);
    }
}

QJsonValue Conversation::call(const QString &method, const QJsonObject &params, int timeoutMs)
{
    try {
        return m_wire->call(method, params, timeoutMs);
    } catch (const RpcError &e) {
        throw OrbitError(QStringLiteral("%1: %2").arg(method, e.message()));
    }
}

QJsonValue Conversation::ext(const QString &what, const QJsonObject &params)
{
    const QString method = QStringLiteral("_orbit/") + what;
    if (m_extensions.isEmpty()) {
        throw OrbitError(QStringLiteral(
                "this orbit has no methods of its own; the orbit checkout and this library do not match"));
    }
    if (!m_extensions.contains(method)) {
        QStringList offered;
        for (const QString &m : m_extensions) {
            offered.append(m.section(QLatin1Char('/'), 1));
        }
        throw OrbitError(QStringLiteral("this orbit does not offer %1; it offers: %2")
                        .arg(what, offered.join(QStringLiteral(", "))));
    }
    return call(method, params, SETUP_TIMEOUT_MS);
}

void Conversation::readHello(const QJsonValue &result)
{
    const QJsonObject res = result.toObject();
    const QJsonValue version = res.value(QStringLiteral("protocolVersion"));
    if (version.toInt() != PROTOCOL_VERSION) {
        throw OrbitError(QStringLiteral("orbit speaks ACP version %1; this library speaks %2")
                        .arg(version.toInt())
                        .arg(PROTOCOL_VERSION));
    }
    m_orbitVersion = res.value(QStringLiteral("agentInfo")).toObject().value(QStringLiteral("version")).toString();
    m_capabilities = res.value(QStringLiteral("agentCapabilities")).toObject();
    const QJsonObject orbit = res.value(QStringLiteral("_meta")).toObject().value(QStringLiteral("orbit")).toObject();
    for (const QJsonValue &m : orbit.value(QStringLiteral("methods")).toArray()) {
        m_extensions.append(m.toString());
    }
}

void Conversation::readModes(const QJsonValue &result)
{
    const QJsonObject modes = result.toObject().value(QStringLiteral("modes")).toObject();
    for (const QJsonValue &v : modes.value(QStringLiteral("availableModes")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        const QString name = o.value(QStringLiteral("name")).toString();
        m_agents.append(
                { id, name.isEmpty() ? id : name, o.value(QStringLiteral("description")).toString(), QString() });
    }
    m_agent = modes.value(QStringLiteral("currentModeId")).toString();
}

void Conversation::readOptions(const QJsonValue &result)
{
    m_options = optionsFrom(result.toObject().value(QStringLiteral("configOptions")).toArray());
}

void Conversation::refreshVision()
{
    if (!m_extensions.contains(QStringLiteral("_orbit/status"))) {
        return;
    }
    QJsonObject prompt = m_capabilities.value(QStringLiteral("promptCapabilities")).toObject();
    prompt.insert(QStringLiteral("image"), status().value(QStringLiteral("vision")).toBool());
    m_capabilities.insert(QStringLiteral("promptCapabilities"), prompt);
}

void Conversation::waitForCommands()
{
    if (m_commandsWaited) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (!m_commandsKnown) {
        m_commandsCond.wait(&m_mutex, 2000);
    }
    m_commandsWaited = true;
}

QList<Command> Conversation::commands() const
{
    const_cast<Conversation *>(this)->waitForCommands();
    return m_commands;
}

bool Conversation::hasCommand(const QString &name) const
{
    for (const Command &c : commands()) {
        if (c.name == name) {
            return true;
        }
    }
    return false;
}

bool Conversation::supportsImages() const
{
    const QJsonObject prompt = m_capabilities.value(QStringLiteral("promptCapabilities")).toObject();
    return prompt.value(QStringLiteral("image")).toBool();
}

QString Conversation::model() const
{
    return const_cast<Conversation *>(this)->status().value(QStringLiteral("model")).toString();
}

bool Conversation::running() const
{
    QMutexLocker lock(&m_mutex);
    return !m_turn.isNull() || !m_stopping.isNull();
}

bool Conversation::alive() const
{
    return m_wire != nullptr && m_wire->alive();
}

void Conversation::switchAgent(const QString &name, int timeoutMs)
{
    call(QStringLiteral("session/set_mode"),
            QJsonObject { { QStringLiteral("sessionId"), m_id }, { QStringLiteral("modeId"), name } }, timeoutMs);
    m_agent = name;
}

void Conversation::setOption(const QString &option, const QJsonValue &value)
{
    QJsonObject params { { QStringLiteral("sessionId"), m_id }, { QStringLiteral("configId"), option },
        { QStringLiteral("value"), value } };
    if (value.isBool()) {
        params.insert(QStringLiteral("type"), QStringLiteral("boolean"));
    }
    const QJsonValue res = call(QStringLiteral("session/set_config_option"), params, SETUP_TIMEOUT_MS);
    readOptions(res);
    refreshVision();
}

void Conversation::switchModel(const QString &name)
{
    bool has = false;
    for (const ConfigOption &o : m_options) {
        if (o.id == QLatin1String("model")) {
            has = true;
            break;
        }
    }
    if (!has) {
        QStringList ids;
        for (const ConfigOption &o : m_options) {
            ids.append(o.id);
        }
        throw OrbitError(
                QStringLiteral("this orbit offers no model option; it offers: %1").arg(ids.join(QStringLiteral(", "))));
    }
    setOption(QStringLiteral("model"), name);
}

// ---- asking ----------------------------------------------------------------

void Conversation::beginAsk(const QString &text, const QList<Image> &images, const QList<Resource> &resources,
        const QStringList &resourcePaths, int timeoutMs)
{
    QString question = text;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_turn.isNull()) {
            throw OrbitError(QStringLiteral("a question is already being answered; wait for it or cancel it"));
        }
        if (!m_stopping.isNull()) {
            throw OrbitError(QStringLiteral("the last question is still being stopped; wait until running is false"));
        }
        if (m_wire == nullptr || !m_wire->alive()) {
            throw OrbitError(m_wire != nullptr ? m_wire->exitMessage() : QStringLiteral("the agent is not running"));
        }
        m_cancelled = false;
        m_timedOut = false;
        m_lastAnswer.clear();
        m_stoppedBecause.clear();
        m_answerText.clear();
        if (!m_briefing.isEmpty()) {
            question = m_briefing.trimmed() + QStringLiteral("\n\n") + text;
            m_briefing.clear();
        }
    }

    if (!images.isEmpty() && !supportsImages()) {
        throw OrbitError(QStringLiteral(
                "the model behind this conversation cannot look at images; check supports_images before sending any"));
    }

    // Build the prompt before the turn is marked running, so a bad resource or
    // image fails the caller without leaving a question open.
    QJsonArray prompt;
    prompt.append(
            QJsonObject { { QStringLiteral("type"), QStringLiteral("text") }, { QStringLiteral("text"), question } });
    for (const Resource &r : resources) {
        QJsonObject res { { QStringLiteral("uri"), r.uri }, { QStringLiteral("text"), r.text } };
        if (!r.mime.isEmpty()) {
            res.insert(QStringLiteral("mimeType"), r.mime);
        }
        prompt.append(QJsonObject {
                { QStringLiteral("type"), QStringLiteral("resource") }, { QStringLiteral("resource"), res } });
    }
    for (const QString &path : resourcePaths) {
        const QString full = QFileInfo(QDir(m_cwd), path).absoluteFilePath();
        QFile f(full);
        if (!f.open(QIODevice::ReadOnly)) {
            throw OrbitError(f.errorString());
        }
        const QString body = QString::fromUtf8(f.readAll());
        QJsonObject res { { QStringLiteral("uri"), QStringLiteral("file://") + full },
            { QStringLiteral("text"), body } };
        const QString mime = resourceMime(QFileInfo(full).suffix().prepend(QLatin1Char('.')));
        if (!mime.isEmpty()) {
            res.insert(QStringLiteral("mimeType"), mime);
        }
        prompt.append(QJsonObject {
                { QStringLiteral("type"), QStringLiteral("resource") }, { QStringLiteral("resource"), res } });
    }
    for (const Image &img : images) {
        QByteArray data;
        QString mime = img.mime;
        if (!img.path.isEmpty()) {
            mime = imageMime(QFileInfo(img.path).suffix().prepend(QLatin1Char('.')));
            if (mime.isEmpty()) {
                throw OrbitError(QStringLiteral("%1: not an image type orbit takes (.gif, .jpeg, .jpg, .png, .webp)")
                                .arg(img.path));
            }
            QFile f(img.path);
            if (!f.open(QIODevice::ReadOnly)) {
                throw OrbitError(f.errorString());
            }
            data = f.readAll();
        } else {
            data = img.data;
        }
        prompt.append(
                QJsonObject { { QStringLiteral("type"), QStringLiteral("image") }, { QStringLiteral("mimeType"), mime },
                        { QStringLiteral("data"), QString::fromLatin1(data.toBase64()) } });
    }

    QSharedPointer<Turn> turn(new Turn());
    {
        QMutexLocker lock(&m_mutex);
        if (!m_turn.isNull()) {
            throw OrbitError(QStringLiteral("a question is already being answered; wait for it or cancel it"));
        }
        m_turn = turn;
    }

    m_wire->sendRequest(QStringLiteral("session/prompt"),
            QJsonObject { { QStringLiteral("sessionId"), m_id }, { QStringLiteral("prompt"), prompt } },
            [this, turn](const QJsonValue &result, const QJsonObject &error) { replied(turn, result, error); });

    if (timeoutMs > 0) {
        QPointer<Conversation> self(this);
        std::thread([self, turn, timeoutMs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
            if (self == nullptr) {
                return;
            }
            {
                QMutexLocker lock(&self->m_mutex);
                if (self->m_turn != turn) {
                    return;
                }
                self->m_timedOut = true;
            }
            self->cancel();
        }).detach();
    }
}

QString Conversation::ask(const QString &text, const EventCallback &onEvent, const QList<Image> &images,
        const QList<Resource> &resources, const QStringList &resourcePaths, int timeoutMs)
{
    beginAsk(text, images, resources, resourcePaths, timeoutMs);
    Event ev;
    while (nextEvent(ev)) {
        if (onEvent) {
            onEvent(ev);
        }
    }
    return m_lastAnswer;
}

bool Conversation::nextEvent(Event &out)
{
    QSharedPointer<Turn> turn;
    {
        QMutexLocker lock(&m_mutex);
        turn = m_turn;
    }
    if (turn.isNull()) {
        return false;
    }
    bool finished = false;
    try {
        for (;;) {
            TurnItem item;
            {
                QMutexLocker lock(&turn->mutex);
                for (;;) {
                    if (!turn->items.isEmpty()) {
                        item = turn->items.takeFirst();
                        break;
                    }
                    if (turn->hasDeadline) {
                        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                turn->deadline - std::chrono::steady_clock::now());
                        if (remaining.count() <= 0) {
                            item.kind = TurnItem::KindDeadline;
                            break;
                        }
                        turn->cond.wait(&turn->mutex, static_cast<unsigned long>(remaining.count()));
                    } else {
                        turn->cond.wait(&turn->mutex);
                    }
                }
            }
            switch (item.kind) {
            case TurnItem::KindEvent: {
                if (item.event.kind == Event::Text) {
                    m_answerText += item.event.text;
                    m_lastAnswer = m_answerText;
                }
                out = item.event;
                return true;
            }
            case TurnItem::KindAsk:
                answerPermission(item.id, item.request);
                break;
            case TurnItem::KindCancel:
                break;
            case TurnItem::KindDeadline:
                // The agent has not said the question is over. It ends here for
                // the caller and stays "stopping" (see endTurn).
                finished = true;
                m_stoppedBecause = m_timedOut ? QStringLiteral("timeout") : QStringLiteral("cancelled");
                endTurn(turn);
                return false;
            case TurnItem::KindDone: {
                finished = true;
                const QString reason = item.result.toObject().value(QStringLiteral("stopReason")).toString();
                m_stoppedBecause = stopReasonWord(reason.isEmpty() ? QStringLiteral("end_turn") : reason);
                if (m_stoppedBecause == QLatin1String("cancelled") && m_timedOut) {
                    m_stoppedBecause = QStringLiteral("timeout");
                }
                endTurn(turn);
                return false;
            }
            case TurnItem::KindError: {
                finished = true;
                m_stoppedBecause = QStringLiteral("error");
                endTurn(turn);
                const QString message = item.error.value(QStringLiteral("message")).toString();
                throw OrbitError(message.isEmpty() ? QStringLiteral("the question failed") : message);
            }
            }
        }
    } catch (...) {
        if (!finished) {
            m_stoppedBecause = QStringLiteral("error");
            endTurn(turn);
        }
        throw;
    }
}

void Conversation::endTurn(const QSharedPointer<Turn> &turn)
{
    QMutexLocker lock(&m_mutex);
    if (m_turn == turn) {
        m_turn.clear();
    }
    if (!turn->replied) {
        m_stopping = turn;
    }
    m_cond.wakeAll();
}

void Conversation::replied(const QSharedPointer<Turn> &turn, const QJsonValue &result, const QJsonObject &error)
{
    {
        QMutexLocker lock(&turn->mutex);
        turn->replied = true;
        TurnItem item;
        if (!error.isEmpty()) {
            item.kind = TurnItem::KindError;
            item.error = error;
        } else {
            item.kind = TurnItem::KindDone;
            item.result = result;
        }
        turn->items.append(item);
        turn->cond.wakeAll();
    }
    QMutexLocker lock(&m_mutex);
    if (m_stopping == turn) {
        m_stopping.clear();
    }
}

void Conversation::cancel()
{
    QSharedPointer<Turn> turn;
    {
        QMutexLocker lock(&m_mutex);
        m_cancelled = true;
        turn = m_turn;
    }
    if (!m_id.isEmpty() && m_wire != nullptr && m_wire->alive()) {
        m_wire->notify(QStringLiteral("session/cancel"), QJsonObject { { QStringLiteral("sessionId"), m_id } });
    }
    if (!turn.isNull()) {
        QMutexLocker lock(&turn->mutex);
        if (!turn->hasDeadline) {
            turn->hasDeadline = true;
            turn->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(CONFIRM_WAIT_MS);
        }
        turn->cond.wakeAll();
    }
}

void Conversation::answerPermission(const QJsonValue &id, const PermissionRequest &request)
{
    bool cancelled = false;
    {
        QMutexLocker lock(&m_mutex);
        cancelled = m_cancelled;
    }
    if (cancelled) {
        m_wire->respond(id,
                QJsonObject { { QStringLiteral("outcome"),
                        QJsonObject { { QStringLiteral("outcome"), QStringLiteral("cancelled") } } } });
        return;
    }
    QString choice;
    try {
        choice = request.optionId(permissions ? permissions(request) : QString::fromLatin1(REJECT));
    } catch (...) {
        m_wire->respond(id,
                QJsonObject { { QStringLiteral("outcome"),
                        QJsonObject { { QStringLiteral("outcome"), QStringLiteral("selected") },
                                { QStringLiteral("optionId"), QString::fromLatin1(REJECT) } } } });
        throw;
    }
    m_wire->respond(id,
            QJsonObject { { QStringLiteral("outcome"),
                    QJsonObject { { QStringLiteral("outcome"), QStringLiteral("selected") },
                            { QStringLiteral("optionId"), choice } } } });
}

// ---- the agent's side ------------------------------------------------------

void Conversation::onNotification(const QString &method, const QJsonObject &params)
{
    if (method != QLatin1String("session/update")) {
        return;
    }
    const QJsonValue sid = params.value(QStringLiteral("sessionId"));
    if (!sid.isUndefined() && !sid.isNull() && !m_id.isEmpty() && sid.toString() != m_id) {
        return;
    }
    const Event ev = eventFrom(params.value(QStringLiteral("update")).toObject());
    const QString name = ev.wireKind;
    if (name == QLatin1String("available_commands_update")) {
        QList<Command> cmds;
        for (const QJsonValue &v : params.value(QStringLiteral("update"))
                        .toObject()
                        .value(QStringLiteral("availableCommands"))
                        .toArray()) {
            const QJsonObject o = v.toObject();
            const QJsonObject input = o.value(QStringLiteral("input")).toObject();
            cmds.append({ o.value(QStringLiteral("name")).toString(), o.value(QStringLiteral("description")).toString(),
                    input.value(QStringLiteral("hint")).toString() });
        }
        QMutexLocker lock(&m_mutex);
        m_commands = cmds;
        m_commandsKnown = true;
        m_commandsCond.wakeAll();
        return;
    }
    if (name == QLatin1String("current_mode_update")) {
        m_agent = params.value(QStringLiteral("update")).toObject().value(QStringLiteral("currentModeId")).toString();
        return;
    }
    if (name == QLatin1String("config_option_update")) {
        m_options = optionsFrom(
                params.value(QStringLiteral("update")).toObject().value(QStringLiteral("configOptions")).toArray());
        return;
    }
    if (name == QLatin1String("session_info_update")) {
        const QJsonObject up = params.value(QStringLiteral("update")).toObject();
        const QString title = up.value(QStringLiteral("title")).toString();
        const QString updated = up.value(QStringLiteral("updatedAt")).toString();
        if (!title.isEmpty()) {
            m_title = title;
        }
        if (!updated.isEmpty()) {
            m_updated = updated;
        }
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (!m_turn.isNull()) {
        enqueueEvent(m_turn, ev);
    } else if (m_stopping.isNull()) {
        m_history.append(ev);
    }
}

void Conversation::enqueueEvent(const QSharedPointer<Turn> &turn, const Event &event)
{
    QMutexLocker lock(&turn->mutex);
    TurnItem item;
    item.kind = TurnItem::KindEvent;
    item.event = event;
    turn->items.append(item);
    turn->cond.wakeAll();
}

void Conversation::onRequest(const QJsonValue &id, const QString &method, const QJsonObject &params)
{
    if (method == QLatin1String("session/request_permission")) {
        const PermissionRequest req = PermissionRequest::fromWire(params);
        QMutexLocker lock(&m_mutex);
        if (!m_turn.isNull()) {
            QMutexLocker turnLock(&m_turn->mutex);
            TurnItem item;
            item.kind = TurnItem::KindAsk;
            item.id = id;
            item.request = req;
            m_turn->items.append(item);
            m_turn->cond.wakeAll();
        } else if (!m_stopping.isNull()) {
            lock.unlock();
            m_wire->respond(id,
                    QJsonObject { { QStringLiteral("outcome"),
                            QJsonObject { { QStringLiteral("outcome"), QStringLiteral("cancelled") } } } });
        } else {
            lock.unlock();
            m_wire->respond(id,
                    QJsonObject { { QStringLiteral("outcome"),
                            QJsonObject { { QStringLiteral("outcome"), QStringLiteral("selected") },
                                    { QStringLiteral("optionId"), QString::fromLatin1(REJECT) } } } });
        }
        return;
    }
    if (method == QLatin1String("fs/read_text_file") || method == QLatin1String("fs/write_text_file")
            || method == QLatin1String("terminal/create") || method == QLatin1String("terminal/output")
            || method == QLatin1String("terminal/wait_for_exit") || method == QLatin1String("terminal/kill")
            || method == QLatin1String("terminal/release") || method == QLatin1String("elicitation/create")) {
        clientCall(id, method, params);
        return;
    }
    m_wire->fail(id, Wire::METHOD_NOT_FOUND, QStringLiteral("this client does not do %1").arg(method));
}

void Conversation::clientCall(const QJsonValue &id, const QString &method, const QJsonObject &params)
{
    QPointer<Conversation> self(this);
    std::thread([self, id, method, params]() {
        if (self == nullptr) {
            return;
        }
        try {
            QJsonObject result;
            if (method == QLatin1String("fs/read_text_file")) {
                if (self->files == nullptr) {
                    throw OrbitError(QStringLiteral("this client does not do %1").arg(method));
                }
                const int line =
                        params.contains(QStringLiteral("line")) ? params.value(QStringLiteral("line")).toInt() : -1;
                const int limit =
                        params.contains(QStringLiteral("limit")) ? params.value(QStringLiteral("limit")).toInt() : -1;
                result.insert(QStringLiteral("content"),
                        self->files->read(params.value(QStringLiteral("path")).toString(), line, limit));
            } else if (method == QLatin1String("fs/write_text_file")) {
                if (self->files == nullptr) {
                    throw OrbitError(QStringLiteral("this client does not do %1").arg(method));
                }
                self->files->write(params.value(QStringLiteral("path")).toString(),
                        params.value(QStringLiteral("content")).toString());
            } else if (method == QLatin1String("terminal/create")) {
                if (self->terminals == nullptr) {
                    throw OrbitError(QStringLiteral("this client does not do %1").arg(method));
                }
                const QJsonArray argsArr = params.value(QStringLiteral("args")).toArray();
                QStringList args;
                for (const QJsonValue &a : argsArr) {
                    args.append(a.toString());
                }
                QHash<QString, QString> env;
                for (const QJsonValue &e : params.value(QStringLiteral("env")).toArray()) {
                    const QJsonObject o = e.toObject();
                    if (!o.value(QStringLiteral("name")).toString().isEmpty()) {
                        env.insert(o.value(QStringLiteral("name")).toString(),
                                o.value(QStringLiteral("value")).toString());
                    }
                }
                const int limit = params.contains(QStringLiteral("outputByteLimit"))
                        ? params.value(QStringLiteral("outputByteLimit")).toInt()
                        : -1;
                const QString tid = self->terminals->create(params.value(QStringLiteral("command")).toString(), args,
                        params.value(QStringLiteral("cwd")).toString(), env, limit);
                result.insert(QStringLiteral("terminalId"), tid);
            } else if (method == QLatin1String("terminal/output")) {
                if (self->terminals == nullptr) {
                    throw OrbitError(QStringLiteral("this client does not do %1").arg(method));
                }
                const TerminalOutput out =
                        self->terminals->output(params.value(QStringLiteral("terminalId")).toString());
                result.insert(QStringLiteral("output"), out.text);
                result.insert(QStringLiteral("truncated"), out.truncated);
                result.insert(QStringLiteral("exitStatus"), exitStatus(out.exitCode, out.signal));
            } else if (method == QLatin1String("terminal/wait_for_exit")) {
                if (self->terminals == nullptr) {
                    throw OrbitError(QStringLiteral("this client does not do %1").arg(method));
                }
                const TerminalExit ended = self->terminals->wait(params.value(QStringLiteral("terminalId")).toString());
                result = exitStatus(ended.exitCode, ended.signal);
            } else if (method == QLatin1String("terminal/kill")) {
                if (self->terminals == nullptr) {
                    throw OrbitError(QStringLiteral("this client does not do %1").arg(method));
                }
                self->terminals->kill(params.value(QStringLiteral("terminalId")).toString());
            } else if (method == QLatin1String("terminal/release")) {
                if (self->terminals == nullptr) {
                    throw OrbitError(QStringLiteral("this client does not do %1").arg(method));
                }
                self->terminals->release(params.value(QStringLiteral("terminalId")).toString());
            } else if (method == QLatin1String("elicitation/create")) {
                if (params.value(QStringLiteral("mode")).toString() != QLatin1String("form")
                        || self->questions == nullptr) {
                    self->m_wire->respond(id, QJsonObject { { QStringLiteral("action"), QStringLiteral("decline") } });
                    return;
                }
                const QList<FormQuestion> asked =
                        formQuestions(params.value(QStringLiteral("requestedSchema")).toObject());
                QList<Question> qs;
                for (const FormQuestion &fq : asked) {
                    qs.append(fq.question);
                }
                const QList<Answer> answers = self->questions->ask(qs);
                if (answers.isEmpty()) {
                    self->m_wire->respond(id, QJsonObject { { QStringLiteral("action"), QStringLiteral("decline") } });
                    return;
                }
                QJsonObject content;
                for (int i = 0; i < asked.size(); ++i) {
                    const Answer answer = i < answers.size() ? answers.at(i) : Answer();
                    const FormQuestion &fq = asked.at(i);
                    const QString typed = answer.typed;
                    if (!fq.question.choices.isEmpty()) {
                        if (!answer.picked.isEmpty()) {
                            if (fq.question.many) {
                                QJsonArray picks;
                                for (const QString &p : answer.picked) {
                                    picks.append(p);
                                }
                                content.insert(fq.key, picks);
                            } else {
                                content.insert(fq.key, answer.picked.first());
                            }
                        }
                        if (!typed.isEmpty()) {
                            content.insert(fq.key + TYPED_FIELD, typed);
                        }
                    } else if (!answer.picked.isEmpty() || !typed.isEmpty()) {
                        content.insert(fq.key, typed.isEmpty() ? answer.picked.join(QLatin1Char(' ')) : typed);
                    }
                }
                self->m_wire->respond(id,
                        QJsonObject { { QStringLiteral("action"), QStringLiteral("accept") },
                                { QStringLiteral("content"), content } });
                return;
            } else {
                self->m_wire->fail(
                        id, Wire::METHOD_NOT_FOUND, QStringLiteral("this client does not do %1").arg(method));
                return;
            }
            self->m_wire->respond(id, result);
        } catch (const std::exception &e) {
            self->m_wire->fail(id, Wire::INTERNAL_ERROR, QString::fromUtf8(e.what()));
        }
    }).detach();
}

void Conversation::onExit(int code)
{
    m_exited = code;
    QSharedPointer<Turn> turn;
    {
        QMutexLocker lock(&m_mutex);
        turn = m_turn;
        m_stopping.clear();
    }
    if (!turn.isNull()) {
        QMutexLocker lock(&turn->mutex);
        TurnItem item;
        item.kind = TurnItem::KindError;
        item.error = QJsonObject { { QStringLiteral("message"), m_wire->exitMessage() } };
        turn->items.append(item);
        turn->cond.wakeAll();
    }
}

// ---- orbit's own methods ---------------------------------------------------

QList<AgentInfo> Conversation::allAgents()
{
    QList<AgentInfo> out;
    for (const QJsonValue &v : ext(QStringLiteral("agents")).toObject().value(QStringLiteral("agents")).toArray()) {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        out.append({ name, name, a.value(QStringLiteral("description")).toString(),
                a.value(QStringLiteral("mode")).toString() });
    }
    return out;
}

QList<ModelInfo> Conversation::models()
{
    QList<ModelInfo> out;
    for (const QJsonValue &v : ext(QStringLiteral("models"), QJsonObject { { QStringLiteral("sessionId"), m_id } })
                    .toObject()
                    .value(QStringLiteral("models"))
                    .toArray()) {
        const QJsonObject m = v.toObject();
        out.append({ m.value(QStringLiteral("id")).toString(), m.value(QStringLiteral("name")).toString(),
                m.value(QStringLiteral("provider")).toString(), m.value(QStringLiteral("arg")).toString(),
                m.value(QStringLiteral("vision")).toBool(), m.value(QStringLiteral("context_limit")).toInt(),
                m.value(QStringLiteral("output_limit")).toInt(), m.value(QStringLiteral("current")).toBool() });
    }
    return out;
}

QList<Skill> Conversation::skills()
{
    QList<Skill> out;
    for (const QJsonValue &v : ext(QStringLiteral("skills")).toObject().value(QStringLiteral("skills")).toArray()) {
        const QJsonObject s = v.toObject();
        out.append({ s.value(QStringLiteral("name")).toString(), s.value(QStringLiteral("description")).toString() });
    }
    return out;
}

void Conversation::useSkill(const QString &name, const QString &text)
{
    QJsonObject params { { QStringLiteral("sessionId"), m_id }, { QStringLiteral("name"), name } };
    if (!text.isEmpty()) {
        params.insert(QStringLiteral("text"), text);
    }
    ext(QStringLiteral("skill_load"), params);
}

QList<Workflow> Conversation::workflows()
{
    QList<Workflow> out;
    for (const QJsonValue &v :
            ext(QStringLiteral("workflows")).toObject().value(QStringLiteral("workflows")).toArray()) {
        const QJsonObject w = v.toObject();
        Workflow wf;
        wf.name = w.value(QStringLiteral("name")).toString();
        wf.description = w.value(QStringLiteral("description")).toString();
        wf.params = w.value(QStringLiteral("params")).toObject();
        wf.disabled = w.value(QStringLiteral("disabled")).toBool();
        for (const QJsonValue &p : w.value(QStringLiteral("problems")).toArray()) {
            wf.problems.append(p.toString());
        }
        out.append(wf);
    }
    return out;
}

QJsonObject Conversation::status()
{
    return ext(QStringLiteral("status"), QJsonObject { { QStringLiteral("sessionId"), m_id } }).toObject();
}

QList<RunningAgent> Conversation::runningAgents()
{
    QList<RunningAgent> out;
    for (const QJsonValue &v : ext(QStringLiteral("running"), QJsonObject { { QStringLiteral("sessionId"), m_id } })
                    .toObject()
                    .value(QStringLiteral("agents"))
                    .toArray()) {
        const QJsonObject a = v.toObject();
        out.append({ a.value(QStringLiteral("id")).toString(), a.value(QStringLiteral("title")).toString(),
                a.value(QStringLiteral("agent")).toString(), a.value(QStringLiteral("status")).toString(),
                a.value(QStringLiteral("depth")).toInt(), a.value(QStringLiteral("session")).toString(),
                a.value(QStringLiteral("parent")).toString() });
    }
    return out;
}

// ---- what orbit runs as a turn ---------------------------------------------

namespace {
QString runCommandLine(Conversation *chat, const QString &name, const QString &line)
{
    if (!chat->hasCommand(name)) {
        QStringList offered;
        for (const Command &c : chat->commands()) {
            offered.append(QStringLiteral("/") + c.name);
        }
        throw OrbitError(QStringLiteral("this orbit has no /%1 command; it offers: %2")
                        .arg(name, offered.join(QStringLiteral(", "))));
    }
    return line;
}
} // namespace

QString Conversation::workflow(const QString &name, const QString &request, const QString &csv, const QString &agent,
        bool header, int verify, const QJsonObject &params, const EventCallback &onEvent)
{
    const QString line = runCommandLine(
            this, QStringLiteral("workflow"), workflowLine(name, request, csv, agent, header, verify, params));
    return ask(line, onEvent);
}

QString Conversation::orchestrate(const QString &request, int verify, const EventCallback &onEvent)
{
    return workflow(QStringLiteral("orchestrate"), request, QString(), QString(), true, verify, {}, onEvent);
}

QString Conversation::batch(const QString &csv, const QString &prompt, const QString &agent, bool header, bool verify,
        const EventCallback &onEvent)
{
    const QString line = runCommandLine(this, QStringLiteral("batch"), batchLine(csv, prompt, agent, header, verify));
    return ask(line, onEvent);
}

QString Conversation::consult(const QString &agent, const QString &question, const EventCallback &onEvent)
{
    return ask(QStringLiteral("@%1 %2").arg(agent, question), onEvent);
}

QString Conversation::brainstorm(const QStringList &agents, const QString &question, const EventCallback &onEvent)
{
    if (agents.size() < 2) {
        throw OrbitError(QStringLiteral("a brainstorm takes two or more agents; use consult for one"));
    }
    QStringList mentions;
    for (const QString &a : agents) {
        mentions.append(QStringLiteral("@") + a);
    }
    return ask(mentions.join(QLatin1Char(' ')) + QLatin1Char(' ') + question, onEvent);
}

QString Conversation::compact(bool full)
{
    runCommandLine(
            this, QStringLiteral("compact"), full ? QStringLiteral("/compact full") : QStringLiteral("/compact"));
    return ask(full ? QStringLiteral("/compact full") : QStringLiteral("/compact")).trimmed();
}

// ---- closing ---------------------------------------------------------------

void Conversation::close()
{
    if (m_wire == nullptr) {
        return;
    }
    if (m_wire->alive() && !m_id.isEmpty() && !m_wire->writing()) {
        try {
            call(QStringLiteral("session/close"), QJsonObject { { QStringLiteral("sessionId"), m_id } }, 10000);
        } catch (const OrbitError &) {
        }
    }
    m_wire->close();
}

} // namespace tqtw_orbitacp
