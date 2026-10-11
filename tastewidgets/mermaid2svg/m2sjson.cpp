/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sjson.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace tqtw_mermaid2svg {

namespace {

const JsonValue &nullValue()
{
    static const JsonValue value;
    return value;
}

class Parser
{
public:
    explicit Parser(const std::string &text)
        : m_text(text)
    {
    }
    bool parse(JsonValue &out)
    {
        skip();
        if (!value(out)) {
            return false;
        }
        skip();
        return m_pos == m_text.size();
    }

private:
    void skip()
    {
        while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) {
            ++m_pos;
        }
    }
    bool value(JsonValue &out)
    {
        skip();
        if (m_pos >= m_text.size()) {
            return false;
        }
        switch (m_text[m_pos]) {
        case '{': return object(out);
        case '[': return array(out);
        case '"': case '\'': {
            std::string s;
            if (!str(s)) {
                return false;
            }
            out = JsonValue::str(s);
            return true;
        }
        case 't':
            if (m_text.compare(m_pos, 4, "true") == 0) {
                m_pos += 4;
                out = JsonValue::boolean(true);
                return true;
            }
            return false;
        case 'f':
            if (m_text.compare(m_pos, 5, "false") == 0) {
                m_pos += 5;
                out = JsonValue::boolean(false);
                return true;
            }
            return false;
        case 'n':
            if (m_text.compare(m_pos, 4, "null") == 0) {
                m_pos += 4;
                out = JsonValue();
                return true;
            }
            return false;
        default: return number(out);
        }
    }
    bool object(JsonValue &out)
    {
        ++m_pos;
        out = JsonValue::object();
        skip();
        if (m_pos < m_text.size() && m_text[m_pos] == '}') {
            ++m_pos;
            return true;
        }
        while (true) {
            skip();
            std::string key;
            if (m_pos < m_text.size() && (m_text[m_pos] == '"' || m_text[m_pos] == '\'')) {
                if (!str(key)) {
                    return false;
                }
            } else {
                // an unquoted key (JSON5-ish): read to ':' 
                std::size_t start = m_pos;
                while (m_pos < m_text.size() && m_text[m_pos] != ':') {
                    ++m_pos;
                }
                key = m_text.substr(start, m_pos - start);
                while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
                    key.pop_back();
                }
            }
            skip();
            if (m_pos >= m_text.size() || m_text[m_pos] != ':') {
                return false;
            }
            ++m_pos;
            JsonValue item;
            if (!value(item)) {
                return false;
            }
            out.set(key, item);
            skip();
            if (m_pos < m_text.size() && m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_text.size() && m_text[m_pos] == '}') {
                ++m_pos;
                return true;
            }
            return false;
        }
    }
    bool array(JsonValue &out)
    {
        ++m_pos;
        out = JsonValue::array();
        skip();
        if (m_pos < m_text.size() && m_text[m_pos] == ']') {
            ++m_pos;
            return true;
        }
        while (true) {
            JsonValue item;
            if (!value(item)) {
                return false;
            }
            out.append(item);
            skip();
            if (m_pos < m_text.size() && m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_text.size() && m_text[m_pos] == ']') {
                ++m_pos;
                return true;
            }
            return false;
        }
    }
    bool str(std::string &out)
    {
        const char quote = m_text[m_pos];
        ++m_pos;
        out.clear();
        while (m_pos < m_text.size()) {
            char c = m_text[m_pos++];
            if (c == quote) {
                return true;
            }
            if (c == '\\' && m_pos < m_text.size()) {
                char e = m_text[m_pos++];
                switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                default: out.push_back(e);
                }
            } else {
                out.push_back(c);
            }
        }
        return false;
    }
    bool number(JsonValue &out)
    {
        const std::size_t start = m_pos;
        while (m_pos < m_text.size()
                && (std::isdigit(static_cast<unsigned char>(m_text[m_pos])) || m_text[m_pos] == '-'
                        || m_text[m_pos] == '+' || m_text[m_pos] == '.' || m_text[m_pos] == 'e'
                        || m_text[m_pos] == 'E')) {
            ++m_pos;
        }
        if (m_pos == start) {
            return false;
        }
        out = JsonValue::number(std::atof(m_text.substr(start, m_pos - start).c_str()));
        return true;
    }

    const std::string &m_text;
    std::size_t m_pos = 0;
};

} // namespace

JsonValue JsonValue::boolean(bool value)
{
    JsonValue v;
    v.m_type = Bool;
    v.m_bool = value;
    return v;
}

JsonValue JsonValue::number(double value)
{
    JsonValue v;
    v.m_type = Number;
    v.m_number = value;
    return v;
}

JsonValue JsonValue::str(const std::string &value)
{
    JsonValue v;
    v.m_type = String;
    v.m_string = value;
    return v;
}

JsonValue JsonValue::array()
{
    JsonValue v;
    v.m_type = Array;
    return v;
}

JsonValue JsonValue::object()
{
    JsonValue v;
    v.m_type = Object;
    return v;
}

JsonValue JsonValue::parse(const std::string &text, bool *ok)
{
    JsonValue out;
    Parser parser(text);
    const bool good = parser.parse(out);
    if (ok != nullptr) {
        *ok = good;
    }
    return good ? out : JsonValue::object();
}

std::string JsonValue::toString() const
{
    if (m_type == String) {
        return m_string;
    }
    if (m_type == Number) {
        std::ostringstream ss;
        if (m_number == static_cast<long long>(m_number)) {
            ss << static_cast<long long>(m_number);
        } else {
            ss << m_number;
        }
        return ss.str();
    }
    if (m_type == Bool) {
        return m_bool ? "true" : "false";
    }
    return std::string();
}

bool JsonValue::toBool() const
{
    if (m_type == Bool) {
        return m_bool;
    }
    if (m_type == Number) {
        return m_number != 0;
    }
    if (m_type == String) {
        return !m_string.empty();
    }
    return false;
}

double JsonValue::toNumber() const
{
    return m_type == Number ? m_number : 0;
}

bool JsonValue::contains(const std::string &key) const
{
    return m_type == Object && m_object.count(key) != 0;
}

const JsonValue &JsonValue::operator[](const std::string &key) const
{
    if (m_type != Object) {
        return nullValue();
    }
    auto it = m_object.find(key);
    return it == m_object.end() ? nullValue() : it->second;
}

const JsonValue &JsonValue::operator[](std::size_t index) const
{
    if (m_type != Array || index >= m_array.size()) {
        return nullValue();
    }
    return m_array[index];
}

void JsonValue::set(const std::string &key, const JsonValue &value)
{
    m_type = Object;
    m_object[key] = value;
}

void JsonValue::append(const JsonValue &value)
{
    m_type = Array;
    m_array.push_back(value);
}

} // namespace tqtw_mermaid2svg
