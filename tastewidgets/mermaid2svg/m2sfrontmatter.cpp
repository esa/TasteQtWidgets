/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sfrontmatter.h"

#include "tqtw_m2serrors.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
#include <sstream>

namespace tqtw_mermaid2svg {

namespace {

std::string trim(const std::string &s)
{
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string ltrim(const std::string &s)
{
    std::size_t i = 0;
    while (i < s.size() && s[i] == ' ') {
        ++i;
    }
    return s.substr(i);
}

//: keyword -> canonical diagram id, in match order (longest first).
const std::vector<std::pair<std::string, std::string>> &diagramKeywords()
{
    static const std::vector<std::pair<std::string, std::string>> k = {
        { "flowchart-elk", "flowchart" }, { "flowchart-v2", "flowchart" }, { "flowchart", "flowchart" },
        { "graph", "flowchart" }, { "sequencediagram", "sequence" }, { "classdiagram-v2", "class" },
        { "classdiagram", "class" }, { "statediagram-v2", "state" }, { "statediagram", "state" },
        { "erdiagram", "er" }, { "gantt", "gantt" }, { "gitgraph", "git" }, { "journey", "journey" },
        { "timeline", "timeline" }, { "mindmap", "mindmap" }, { "quadrantchart", "quadrant" },
        { "requirementdiagram", "requirement" }, { "packet-beta", "packet" }, { "packet", "packet" },
        { "radar-beta", "radar" }, { "kanban", "kanban" }, { "xychart-beta", "xychart" },
        { "xychart", "xychart" }, { "sankey-beta", "sankey" }, { "sankey", "sankey" }, { "pie", "pie" },
    };
    return k;
}

const std::map<std::string, std::string> &unsupportedKeywords()
{
    static const std::map<std::string, std::string> k = {
        { "zenuml", "requires a separate rendering engine" },
        { "architecture-beta", "requires icon packs fetched over the network" },
        { "architecture", "requires icon packs fetched over the network" },
        { "c4context", "not implemented" }, { "c4container", "not implemented" },
        { "c4component", "not implemented" }, { "c4dynamic", "not implemented" },
        { "c4deployment", "not implemented" }, { "block-beta", "not implemented" },
        { "treemap-beta", "not implemented" }, { "treeview-beta", "not implemented" },
        { "cynefin", "not implemented" }, { "eventmodeling", "not implemented" },
        { "wardley-beta", "not implemented" }, { "swimlanes", "requires the swimlane flowchart layout" },
        { "railroad-beta", "not implemented" }, { "railroad-ebnf-beta", "not implemented" },
        { "railroad-abnf-beta", "not implemented" }, { "railroad-peg-beta", "not implemented" },
        { "venn-beta", "not implemented" }, { "ishikawa-beta", "not implemented" }, { "info", "not implemented" },
    };
    return k;
}

//: A deliberately small YAML subset: scalars and one level of nesting.
JsonValue coerce(const std::string &rawIn)
{
    const std::string raw = trim(rawIn);
    if (raw.empty()) {
        return JsonValue::str("");
    }
    if ((raw.front() == '"' || raw.front() == '\'') && raw.size() > 1 && raw.back() == raw.front()) {
        return JsonValue::str(raw.substr(1, raw.size() - 2));
    }
    const std::string low = lower(raw);
    if (low == "true" || low == "yes") {
        return JsonValue::boolean(true);
    }
    if (low == "false" || low == "no") {
        return JsonValue::boolean(false);
    }
    if (low == "null" || low == "~") {
        return JsonValue();
    }
    if (raw.front() == '[' || raw.front() == '{') {
        std::string relaxed = raw;
        std::replace(relaxed.begin(), relaxed.end(), '\'', '"');
        bool ok = false;
        JsonValue parsed = JsonValue::parse(relaxed, &ok);
        return ok ? parsed : JsonValue::str(raw);
    }
    // number?
    bool numeric = !raw.empty();
    std::size_t i = 0;
    if (raw[i] == '-' || raw[i] == '+') {
        ++i;
    }
    int digits = 0;
    int dots = 0;
    for (; i < raw.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(raw[i]))) {
            ++digits;
        } else if (raw[i] == '.') {
            ++dots;
        } else {
            numeric = false;
            break;
        }
    }
    if (numeric && digits > 0 && dots <= 1) {
        return JsonValue::number(std::atof(raw.c_str()));
    }
    return JsonValue::str(raw);
}

//: Parse indented key: value YAML with nested maps and "- " lists, using a
//: stack of pointers (std::map nodes are stable, so ancestors stay valid).
JsonValue parseYamlSubset(const std::string &text)
{
    JsonValue root = JsonValue::object();

    struct Frame
    {
        JsonValue *node = nullptr;
        int indent = -1;
        bool isList = false;
        //: The key under which this container sits in its parent, if it was
        //: created as an empty placeholder and may yet be promoted to a list.
        std::string keyInParent;
        JsonValue *parent = nullptr;
    };
    std::vector<Frame> stack;
    Frame rootFrame;
    rootFrame.node = &root;
    stack.push_back(rootFrame);

    std::istringstream ss(text);
    std::string rawLine;
    while (std::getline(ss, rawLine)) {
        if (trim(rawLine).empty() || ltrim(rawLine).rfind("#", 0) == 0) {
            continue;
        }
        int indent = 0;
        while (indent < static_cast<int>(rawLine.size()) && rawLine[indent] == ' ') {
            ++indent;
        }
        const std::string line = trim(rawLine);
        while (stack.size() > 1 && indent <= stack.back().indent) {
            stack.pop_back();
        }
        Frame &container = stack.back();

        if (line.rfind("- ", 0) == 0) {
            const JsonValue item = coerce(line.substr(2));
            if (container.isList) {
                container.node->arrayRef().push_back(item);
            } else if (container.parent != nullptr && !container.keyInParent.empty()) {
                // Promote the empty placeholder object to a list.
                JsonValue list = JsonValue::array();
                list.arrayRef().push_back(item);
                container.parent->objectRef()[container.keyInParent] = list;
                JsonValue *listRef = container.parent->find(container.keyInParent);
                container.node = listRef;
                container.isList = true;
            }
            continue;
        }

        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (!container.node->isObject()) {
            continue;
        }
        if (value.empty()) {
            container.node->objectRef()[key] = JsonValue::object();
            Frame child;
            child.node = container.node->find(key);
            child.indent = indent;
            child.isList = false;
            child.keyInParent = key;
            child.parent = container.node;
            stack.push_back(child);
        } else if (value == "[]") {
            container.node->objectRef()[key] = JsonValue::array();
        } else {
            container.node->objectRef()[key] = coerce(value);
        }
    }

    // Drop empty placeholder objects that never received a value.
    std::function<void(JsonValue &)> prune = [&](JsonValue &node) {
        if (!node.isObject()) {
            return;
        }
        std::vector<std::string> empty;
        for (auto &entry : node.objectRef()) {
            if (entry.second.isObject() && entry.second.asObject().empty()) {
                empty.push_back(entry.first);
            } else {
                prune(entry.second);
            }
        }
        for (const std::string &key : empty) {
            node.erase(key);
        }
    };
    prune(root);
    return root;
}

//: The inside of %%{init: {...}}%%.
JsonValue parseDirectiveBody(const std::string &bodyIn)
{
    std::string body = trim(bodyIn);
    for (const char *prefix : { "initialize", "init", "wrap" }) {
        const std::string p = prefix;
        if (body.size() >= p.size() && lower(body.substr(0, p.size())) == p) {
            body = ltrim(body.substr(p.size()));
            if (!body.empty() && body.front() == ':') {
                body = ltrim(body.substr(1));
            }
            break;
        }
    }
    if (body.empty()) {
        return JsonValue::object();
    }
    if (body.front() != '{') {
        body = "{" + body;
    }
    bool ok = false;
    JsonValue parsed = JsonValue::parse(body, &ok);
    if (ok && parsed.isObject()) {
        return parsed;
    }
    // Relax single quotes and unquoted keys, then retry.
    std::string relaxed = body;
    std::replace(relaxed.begin(), relaxed.end(), '\'', '"');
    JsonValue parsed2 = JsonValue::parse(relaxed, &ok);
    return (ok && parsed2.isObject()) ? parsed2 : JsonValue::object();
}

} // namespace

std::vector<std::string> Source::lines() const
{
    std::vector<std::string> out;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            out.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(current);
    return out;
}

Source preprocess(const std::string &textIn, Diagnostics *diagnostics)
{
    std::string text = textIn;
    // Normalise newlines.
    std::string normalised;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                continue;
            }
            normalised.push_back('\n');
        } else {
            normalised.push_back(text[i]);
        }
    }
    text = normalised;

    Source source;
    source.config = JsonValue::object();

    // frontmatter: ^\s*---\n ... \n---\n
    {
        const std::string startTrim = ltrim(text);
        if (startTrim.rfind("---", 0) == 0 && (startTrim.size() == 3 || startTrim[3] == '\n')) {
            const std::string after = startTrim.substr(3);
            std::size_t nl = after.find('\n');
            if (nl != std::string::npos) {
                const std::string body = after.substr(0, nl);
                const std::string rest = after.substr(nl + 1);
                // the closing --- must be at the start of a line
                std::size_t closeLine = std::string::npos;
                std::size_t pos = 0;
                while (pos <= rest.size()) {
                    const std::size_t lineEnd = rest.find('\n', pos);
                    const std::string line = rest.substr(
                            pos, lineEnd == std::string::npos ? std::string::npos : lineEnd - pos);
                    if (trim(line) == "---" || trim(line) == "---\r") {
                        closeLine = pos;
                        break;
                    }
                    if (lineEnd == std::string::npos) {
                        break;
                    }
                    pos = lineEnd + 1;
                }
                if (closeLine != std::string::npos) {
                    std::string block;
                    pos = 0;
                    while (pos < closeLine) {
                        const std::size_t le = rest.find('\n', pos);
                        block += rest.substr(pos, le - pos) + "\n";
                        pos = le + 1;
                    }
                    // compute offset: lines consumed before the body
                    const std::size_t consumed = text.size() - rest.size() + closeLine;
                    int lines = 0;
                    for (std::size_t k = 0; k < consumed && k < text.size(); ++k) {
                        if (text[k] == '\n') {
                            ++lines;
                        }
                    }
                    // skip the closing --- line too
                    const std::size_t bodyStart = rest.find('\n', closeLine);
                    source.lineOffset = lines + 1;
                    text = bodyStart == std::string::npos ? std::string() : rest.substr(bodyStart + 1);
                    JsonValue meta = parseYamlSubset(block);
                    if (meta.contains("title") && !meta["title"].isNull()) {
                        source.hasTitle = true;
                        source.title = meta["title"].toString();
                    }
                    if (meta.contains("displayMode") && meta["displayMode"].type() == JsonValue::String) {
                        source.hasDisplayMode = true;
                        source.displayMode = meta["displayMode"].toString();
                    }
                    if (meta.contains("config") && meta["config"].isObject()) {
                        source.config = meta["config"];
                    }
                }
            }
        }
    }

    // %%{init: ...}%% directives: collect, then blank their lines.
    {
        std::string out;
        std::size_t i = 0;
        while (i < text.size()) {
            if (text.compare(i, 2, "%%") == 0) {
                const std::size_t brace = text.find('{', i);
                if (brace != std::string::npos) {
                    const std::size_t closeBrace = text.find('}', brace);
                    const std::size_t closeDirective = closeBrace == std::string::npos
                            ? std::string::npos
                            : text.find("%%", closeBrace);
                    if (closeBrace != std::string::npos && closeDirective != std::string::npos) {
                        const std::string body = text.substr(brace + 1, closeBrace - brace - 1);
                        JsonValue parsed = parseDirectiveBody(body);
                        JsonValue inner = parsed.contains("init") && parsed["init"].isObject()
                                ? parsed["init"]
                                : (parsed.contains("initialize") && parsed["initialize"].isObject()
                                                ? parsed["initialize"]
                                                : parsed);
                        if (inner.isObject()) {
                            for (const auto &entry : inner.asObject()) {
                                source.config.set(entry.first, entry.second);
                            }
                        }
                        // Blank out the directive, keeping its newlines.
                        const std::string whole = text.substr(i, closeDirective + 2 - i);
                        for (char c : whole) {
                            out.push_back(c == '\n' ? '\n' : ' ');
                        }
                        i = closeDirective + 2;
                        continue;
                    }
                }
            }
            out.push_back(text[i]);
            ++i;
        }
        text = out;
    }

    // accDescr { ... } block.
    {
        // match a line: ^[ \t]*accDescr[ \t]*{ (body) }[ \t]*$
        std::istringstream ss(text);
        std::string rebuilt;
        std::string line;
        bool replaced = false;
        while (std::getline(ss, line)) {
            const std::string t = trim(line);
            if (!replaced && t.size() > 8 && lower(t.substr(0, 7)) == "accdescr"
                    && t.find('{') != std::string::npos && t.find('}') != std::string::npos) {
                const std::size_t open = t.find('{');
                const std::size_t close = t.rfind('}');
                std::string body = t.substr(open + 1, close - open - 1);
                std::istringstream bs(body);
                std::string word;
                std::string joined;
                while (bs >> word) {
                    if (!joined.empty()) {
                        joined += " ";
                    }
                    joined += word;
                }
                source.hasAccDescr = true;
                source.accDescr = joined;
                rebuilt += std::string(line.size(), ' ') + "\n";
                replaced = true;
            } else {
                rebuilt += line + "\n";
            }
        }
        if (!text.empty() && text.back() != '\n') {
            // getline dropped a trailing unterminated line; rebuilt has it.
        }
        text = rebuilt;
    }

    // Comments: %% lines (but not %%{ directives, already blanked).
    {
        std::istringstream ss(text);
        std::string out;
        std::string line;
        while (std::getline(ss, line)) {
            const std::string t = trim(line);
            if (t.rfind("%%", 0) == 0 && t.rfind("%%{", 0) != 0) {
                out += "\n";
            } else {
                out += line + "\n";
            }
        }
        text = out;
    }

    source.text = text;
    if (diagnostics != nullptr) {
        (void)diagnostics;
    }
    return source;
}

std::pair<std::string, std::string> detectType(const Source &source)
{
    const std::vector<std::string> lines = source.lines();
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string stripped = trim(lines[index]);
        if (stripped.empty()) {
            continue;
        }
        const std::string lowered = lower(stripped);
        for (const auto &entry : unsupportedKeywords()) {
            if (lowered == entry.first || lowered.rfind(entry.first + " ", 0) == 0
                    || lowered.rfind(entry.first + "\t", 0) == 0) {
                throw UnsupportedDiagramError(entry.first, entry.second);
            }
        }
        for (const auto &entry : diagramKeywords()) {
            const std::string &keyword = entry.first;
            if (lowered == keyword) {
                return { entry.second, keyword };
            }
            if (lowered.rfind(keyword, 0) == 0 && lowered.size() > keyword.size()
                    && (lowered[keyword.size()] == ' ' || lowered[keyword.size()] == ':')) {
                return { entry.second, keyword };
            }
        }
        std::set<std::string> ids;
        for (const auto &entry : diagramKeywords()) {
            ids.insert(entry.second);
        }
        std::string expected;
        for (const std::string &id : ids) {
            if (!expected.empty()) {
                expected += ", ";
            }
            expected += id;
        }
        throw UnknownDiagramError("no diagram type detected in '" + stripped.substr(0, stripped.find(' '))
                        + "'; expected one of: " + expected,
                source.absLine(static_cast<int>(index) + 1));
    }
    throw UnknownDiagramError("empty diagram definition", 1, 1);
}

} // namespace tqtw_mermaid2svg
