#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace yt {

enum class JsonType {
    NUL,
    BOOLEAN,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT
};

class JsonValue {
public:
    JsonValue() : m_type(JsonType::NUL) {}
    explicit JsonValue(bool b) : m_type(JsonType::BOOLEAN), m_bool(b) {}
    explicit JsonValue(double n) : m_type(JsonType::NUMBER), m_number(n) {}
    explicit JsonValue(const std::string& s) : m_type(JsonType::STRING), m_string(s) {}
    explicit JsonValue(const char* s) : m_type(JsonType::STRING), m_string(s ? s : "") {}
    explicit JsonValue(JsonType t) : m_type(t) {}

    JsonType type() const { return m_type; }
    bool isNull() const { return m_type == JsonType::NUL; }
    bool isBool() const { return m_type == JsonType::BOOLEAN; }
    bool isNumber() const { return m_type == JsonType::NUMBER; }
    bool isString() const { return m_type == JsonType::STRING; }
    bool isArray() const { return m_type == JsonType::ARRAY; }
    bool isObject() const { return m_type == JsonType::OBJECT; }

    bool asBool(bool def = false) const { return isBool() ? m_bool : def; }
    double asDouble(double def = 0.0) const { return isNumber() ? m_number : def; }
    int asInt(int def = 0) const { return isNumber() ? static_cast<int>(m_number) : def; }
    uint64_t asUInt64(uint64_t def = 0) const { return isNumber() ? static_cast<uint64_t>(m_number) : def; }
    const std::string& asString(const std::string& def = "") const { return isString() ? m_string : def; }

    const std::vector<JsonValue>& asArray() const { return m_array; }
    std::vector<JsonValue>& asArray() { return m_array; }

    const std::unordered_map<std::string, JsonValue>& asObject() const { return m_object; }
    std::unordered_map<std::string, JsonValue>& asObject() { return m_object; }

    bool contains(const std::string& key) const;
    const JsonValue& operator[](const std::string& key) const;
    const JsonValue& operator[](size_t index) const;

    std::string getString(const std::string& key, const std::string& def = "") const;
    int getInt(const std::string& key, int def = 0) const;
    uint64_t getUInt64(const std::string& key, uint64_t def = 0) const;
    double getDouble(const std::string& key, double def = 0.0) const;
    bool getBool(const std::string& key, bool def = false) const;

private:
    friend class JsonParser;
    JsonType m_type{JsonType::NUL};
    bool m_bool{false};
    double m_number{0.0};
    std::string m_string;
    std::vector<JsonValue> m_array;
    std::unordered_map<std::string, JsonValue> m_object;
    static const JsonValue s_null;
};

class JsonParser {
public:
    static bool parse(const std::string& jsonText, JsonValue& outRoot);

private:
    explicit JsonParser(const std::string& text) : m_text(text), m_index(0) {}

    bool parseValue(JsonValue& val);
    bool parseObject(JsonValue& val);
    bool parseArray(JsonValue& val);
    bool parseString(std::string& str);
    bool parseNumber(double& num);
    bool parseLiteral(const std::string& lit, JsonValue& val, JsonType type);

    void skipWhitespace();
    char peek() const;
    char get();
    bool eof() const;

    const std::string& m_text;
    size_t m_index{0};
    int m_depth{0};
};

} // namespace yt
