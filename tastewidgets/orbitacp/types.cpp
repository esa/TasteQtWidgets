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

#include "tqtw_types.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace tqtw_orbitacp {

const char *const AGENT_ENV = "ORBIT_ACP_AGENT";
const char *const ONCE = "once";
const char *const ALWAYS = "always";
const char *const REJECT = "reject";

const QStringList PermissionRequest::SHELL_PERMISSIONS = { QStringLiteral("bash") };

OrbitError::OrbitError(const QString &message)
    : std::runtime_error(message.toStdString())
    , m_message(message)
{
}

OrbitError::OrbitError(const OrbitError &other)
    : std::runtime_error(other)
    , m_message(other.m_message)
{
}

OrbitError::~OrbitError() = default;

QString OrbitError::message() const
{
    return m_message;
}

AgentNotFound::AgentNotFound(const QString &message)
    : OrbitError(message)
{
}

AgentNotFound::AgentNotFound(const AgentNotFound &other)
    : OrbitError(other)
{
}

AgentNotFound::~AgentNotFound() = default;

FileHandler::~FileHandler() = default;
TerminalHandler::~TerminalHandler() = default;
QuestionHandler::~QuestionHandler() = default;

// ---- PermissionRequest -----------------------------------------------------

QString PermissionRequest::command() const
{
    if (SHELL_PERMISSIONS.contains(permission) && !patterns.isEmpty()) {
        return patterns.first();
    }
    return {};
}

QString PermissionRequest::optionId(const QString &answer) const
{
    for (const Option &o : options) {
        if (!o.id.isEmpty() && (answer == o.id || answer == o.kind)) {
            return o.id;
        }
    }
    return QString::fromLatin1(REJECT);
}

PermissionRequest PermissionRequest::fromWire(const QJsonObject &params)
{
    PermissionRequest req;
    req.raw = params;
    const QJsonObject call = params.value(QStringLiteral("toolCall")).toObject();
    const QJsonObject raw = call.value(QStringLiteral("rawInput")).toObject();
    for (const QJsonValue &v : params.value(QStringLiteral("options")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString id = o.value(QStringLiteral("optionId")).toString();
        if (id.isEmpty() && !o.contains(QStringLiteral("optionId"))) {
            continue;
        }
        const QString name = o.value(QStringLiteral("name")).toString();
        req.options.append({ id, name.isEmpty() ? id : name, o.value(QStringLiteral("kind")).toString() });
    }
    req.title = call.value(QStringLiteral("title")).toString();
    const QString kind = call.value(QStringLiteral("kind")).toString();
    req.toolKind = kind.isEmpty() ? QStringLiteral("other") : kind;
    req.permission = raw.value(QStringLiteral("permission")).toString();
    for (const QJsonValue &p : raw.value(QStringLiteral("patterns")).toArray()) {
        req.patterns.append(p.toString());
    }
    req.toolId = call.value(QStringLiteral("toolCallId")).toString();
    return req;
}

// ---- helpers ---------------------------------------------------------------

namespace {

int toInt(const QJsonValue &v)
{
    // A number that is not one ("many", null) reads as 0.
    return v.isDouble() ? static_cast<int>(v.toDouble()) : 0;
}

QString contentText(const QJsonObject &up)
{
    return up.value(QStringLiteral("content")).toObject().value(QStringLiteral("text")).toString();
}

//: The `_meta.orbit.task` an update carries, or an empty object.
QJsonObject taskMeta(const QJsonObject &up)
{
    const QJsonObject orbit = up.value(QStringLiteral("_meta")).toObject().value(QStringLiteral("orbit")).toObject();
    const QJsonValue task = orbit.value(QStringLiteral("task"));
    return task.isObject() ? task.toObject() : QJsonObject();
}

Event makeTool(const QJsonObject &up)
{
    Event ev;
    const QJsonObject task = taskMeta(up);
    if (!task.isEmpty()) {
        ev.kind = Event::Task;
        ev.id = task.value(QStringLiteral("id")).toString();
        ev.title = task.value(QStringLiteral("title")).toString();
        ev.agent = task.value(QStringLiteral("agent")).toString();
        ev.status = task.value(QStringLiteral("status")).toString();
        ev.depth = toInt(task.value(QStringLiteral("depth")));
        ev.session = task.value(QStringLiteral("session")).toString();
        ev.parent = task.value(QStringLiteral("parent")).toString();
        ev.raw = up;
        return ev;
    }
    ev.kind = Event::Tool;
    QString output;
    for (const QJsonValue &b : up.value(QStringLiteral("content")).toArray()) {
        const QJsonObject block = b.toObject();
        const QString type = block.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("diff") && !ev.hasDiff) {
            ev.hasDiff = true;
            ev.diff.path = block.value(QStringLiteral("path")).toString();
            ev.diff.oldText = block.value(QStringLiteral("oldText")).toString();
            ev.diff.newText = block.value(QStringLiteral("newText")).toString();
        } else if (type == QLatin1String("terminal") && ev.terminal.isEmpty()) {
            ev.terminal = block.value(QStringLiteral("terminalId")).toString();
        } else if (type == QLatin1String("content")) {
            const QJsonObject inner = block.value(QStringLiteral("content")).toObject();
            if (inner.value(QStringLiteral("type")).toString() == QLatin1String("text")) {
                output += inner.value(QStringLiteral("text")).toString();
            }
        }
    }
    if (output.isEmpty() && up.value(QStringLiteral("rawOutput")).isString()) {
        output = up.value(QStringLiteral("rawOutput")).toString();
    }
    for (const QJsonValue &loc : up.value(QStringLiteral("locations")).toArray()) {
        const QString path = loc.toObject().value(QStringLiteral("path")).toString();
        if (!path.isEmpty()) {
            ev.files.append(path);
        }
    }
    ev.id = up.value(QStringLiteral("toolCallId")).toString();
    ev.title = up.value(QStringLiteral("title")).toString();
    const QString kind = up.value(QStringLiteral("kind")).toString();
    ev.toolKind = kind.isEmpty() ? QStringLiteral("other") : kind;
    const QString status = up.value(QStringLiteral("status")).toString();
    ev.status = status.isEmpty() ? QStringLiteral("pending") : status;
    const QJsonValue rawInput = up.value(QStringLiteral("rawInput"));
    ev.input = rawInput.isObject() ? rawInput.toObject() : QJsonObject();
    ev.output = output;
    ev.raw = up;
    return ev;
}

Event makePlan(const QJsonObject &up)
{
    Event ev;
    ev.kind = Event::Plan;
    for (const QJsonValue &e : up.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject entry = e.toObject();
        const QString status = entry.value(QStringLiteral("status")).toString();
        const QString priority = entry.value(QStringLiteral("priority")).toString();
        ev.steps.append({ entry.value(QStringLiteral("content")).toString(),
                status.isEmpty() ? QStringLiteral("pending") : status,
                priority.isEmpty() ? QStringLiteral("medium") : priority });
    }
    ev.raw = up;
    return ev;
}

QList<Choice> choicesOf(const QJsonArray &options)
{
    QList<Choice> out;
    for (const QJsonValue &v : options) {
        const QJsonObject o = v.toObject();
        if (o.contains(QStringLiteral("options"))) {
            out.append(choicesOf(o.value(QStringLiteral("options")).toArray()));
        } else {
            const QString value = o.value(QStringLiteral("value")).toString();
            QString name = o.value(QStringLiteral("name")).toString();
            out.append({ value, name.isEmpty() ? value : name });
        }
    }
    return out;
}

} // namespace

Event eventFrom(const QJsonObject &update)
{
    Event ev;
    const QString name = update.value(QStringLiteral("sessionUpdate")).toString();
    ev.raw = update;
    ev.wireKind = name;
    if (name == QLatin1String("agent_message_chunk")) {
        ev.kind = Event::Text;
        ev.text = contentText(update);
    } else if (name == QLatin1String("agent_thought_chunk")) {
        ev.kind = Event::Thought;
        ev.text = contentText(update);
    } else if (name == QLatin1String("user_message_chunk")) {
        ev.kind = Event::UserText;
        ev.text = contentText(update);
    } else if (name == QLatin1String("tool_call") || name == QLatin1String("tool_call_update")) {
        ev = makeTool(update);
    } else if (name == QLatin1String("plan")) {
        ev = makePlan(update);
    } else if (name == QLatin1String("usage_update")) {
        ev.kind = Event::Usage;
        ev.used = toInt(update.value(QStringLiteral("used")));
        ev.size = toInt(update.value(QStringLiteral("size")));
    } else {
        ev.kind = Event::Raw;
    }
    return ev;
}

QList<ConfigOption> optionsFrom(const QJsonArray &rows)
{
    QList<ConfigOption> out;
    for (const QJsonValue &v : rows) {
        const QJsonObject o = v.toObject();
        if (o.isEmpty()) {
            continue;
        }
        ConfigOption opt;
        opt.id = o.value(QStringLiteral("id")).toString();
        opt.name = o.value(QStringLiteral("name")).toString();
        opt.category = o.value(QStringLiteral("category")).toString();
        const QString type = o.value(QStringLiteral("type")).toString();
        opt.type = type.isEmpty() ? QStringLiteral("select") : type;
        const QJsonValue value = o.value(QStringLiteral("currentValue"));
        if (opt.type == QLatin1String("boolean")) {
            opt.value = value.toBool();
        } else {
            opt.value = value.isNull() ? QJsonValue(QString()) : QJsonValue(value.toString());
        }
        if (opt.type == QLatin1String("select")) {
            opt.choices = choicesOf(o.value(QStringLiteral("options")).toArray());
        }
        out.append(opt);
    }
    return out;
}

QList<FormQuestion> formQuestions(const QJsonObject &schema)
{
    QList<FormQuestion> out;
    const QJsonObject props = schema.value(QStringLiteral("properties")).toObject();
    const QString typedField = QStringLiteral("_text");
    for (auto it = props.begin(); it != props.end(); ++it) {
        const QString key = it.key();
        const QJsonObject spec = it.value().toObject();
        if (key.endsWith(typedField) && !out.isEmpty() && out.last().key == key.chopped(typedField.size())) {
            out.last().question.freeText = true;
            continue;
        }
        Question q;
        q.freeText = true;
        const QString type = spec.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("string") || type == QLatin1String("array")) {
            const QJsonObject items =
                    type == QLatin1String("array") ? spec.value(QStringLiteral("items")).toObject() : spec;
            const QJsonArray enumValues = items.value(QStringLiteral("enum")).toArray();
            if (!enumValues.isEmpty()) {
                // A `oneOf` entry's title carries a note after " — ".
                QHash<QString, QString> notes;
                for (const QJsonValue &e : items.value(QStringLiteral("oneOf")).toArray()) {
                    const QJsonObject entry = e.toObject();
                    const QString title = entry.value(QStringLiteral("title")).toString();
                    const QString cst = entry.value(QStringLiteral("const")).toString();
                    const QString prefix = cst + QStringLiteral(" — ");
                    if (title.startsWith(prefix)) {
                        notes.insert(cst, title.mid(prefix.size()));
                    }
                }
                for (const QJsonValue &v : enumValues) {
                    const QString value = v.toString();
                    q.choices.append({ value, notes.value(value) });
                }
                q.many = type == QLatin1String("array");
                q.freeText = false;
            }
        }
        q.text = spec.value(QStringLiteral("description")).toString(spec.value(QStringLiteral("title")).toString(key));
        const QString title = spec.value(QStringLiteral("title")).toString();
        q.topic = (!title.isEmpty() && title != q.text) ? title : QString();
        out.append({ q, key });
    }
    return out;
}

} // namespace tqtw_orbitacp
