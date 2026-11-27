/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// A small JSON value, enough for frontmatter and `%%{init}%%` directives.
// A C++ port of the subset of json that mermaid2svg's frontmatter.py uses.

#include <map>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

class JsonValue
{
public:
    enum Type { Null, Bool, Number, String, Array, Object };

    JsonValue() = default;
    static JsonValue boolean(bool value);
    static JsonValue number(double value);
    static JsonValue str(const std::string &value);
    static JsonValue array();
    static JsonValue object();

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Null; }
    bool isObject() const { return m_type == Object; }
    bool isArray() const { return m_type == Array; }

    //: Parse; on failure `ok` is false and an empty object is returned.
    static JsonValue parse(const std::string &text, bool *ok = nullptr);

    std::string toString() const;
    bool toBool() const;
    double toNumber() const;

    bool contains(const std::string &key) const;
    const JsonValue &operator[](const std::string &key) const;
    const JsonValue &operator[](std::size_t index) const;
    void set(const std::string &key, const JsonValue &value);
    void append(const JsonValue &value);

    const std::map<std::string, JsonValue> &asObject() const { return m_object; }
    const std::vector<JsonValue> &asArray() const { return m_array; }

    //: Mutable access, for building a value tree (frontmatter, directives).
    void makeObject() { m_type = Object; }
    void makeArray() { m_type = Array; }
    std::map<std::string, JsonValue> &objectRef() { m_type = Object; return m_object; }
    std::vector<JsonValue> &arrayRef() { m_type = Array; return m_array; }
    JsonValue *find(const std::string &key) { auto it = m_object.find(key); return it == m_object.end() ? nullptr : &it->second; }
    void erase(const std::string &key) { m_object.erase(key); }

private:
    Type m_type = Null;
    bool m_bool = false;
    double m_number = 0;
    std::string m_string;
    std::vector<JsonValue> m_array;
    std::map<std::string, JsonValue> m_object;
};

} // namespace tqtw_mermaid2svg
