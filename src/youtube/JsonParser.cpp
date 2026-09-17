#include "youtube/JsonParser.h"
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace yt {

const JsonValue JsonValue::s_null;

bool JsonValue::contains(const std::string& key) const {
    if (!isObject()) return false;
    return m_object.find(key) != m_object.end();
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (!isObject()) return s_null;
    auto it = m_object.find(key);
    if (it != m_object.end()) {
        return it->second;
    }
    return s_null;
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (!isArray() || index >= m_array.size()) {
        return s_null;
    }
    return m_array[index];
}

std::string JsonValue::getString(const std::string& key, const std::string& def) const {
    if (!isObject()) return def;
    auto it = m_object.find(key);
    if (it != m_object.end() && it->second.isString()) {
        return it->second.asString();
    }
    return def;
}

int JsonValue::getInt(const std::string& key, int def) const {
    if (!isObject()) return def;
    auto it = m_object.find(key);
    if (it != m_object.end() && it->second.isNumber()) {
        return it->second.asInt();
    }
    return def;
}

uint64_t JsonValue::getUInt64(const std::string& key, uint64_t def) const {
    if (!isObject()) return def;
    auto it = m_object.find(key);
    if (it != m_object.end() && it->second.isNumber()) {
        return it->second.asUInt64();
    }
    return def;
}

double JsonValue::getDouble(const std::string& key, double def) const {
    if (!isObject()) return def;
    auto it = m_object.find(key);
    if (it != m_object.end() && it->second.isNumber()) {
        return it->second.asDouble();
    }
    return def;
}

bool JsonValue::getBool(const std::string& key, bool def) const {
    if (!isObject()) return def;
    auto it = m_object.find(key);
    if (it != m_object.end() && it->second.isBool()) {
        return it->second.asBool();
    }
    return def;
}

// Parser Implementation
bool JsonParser::parse(const std::string& jsonText, JsonValue& outRoot) {
    if (jsonText.empty()) return false;
    JsonParser p(jsonText);
    p.skipWhitespace();
    if (!p.parseValue(outRoot)) {
        return false;
    }
    p.skipWhitespace();
    return true;
}

void JsonParser::skipWhitespace() {
    while (!eof()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            get();
        } else {
            break;
        }
    }
}

char JsonParser::peek() const {
    if (m_index < m_text.size()) {
        return m_text[m_index];
    }
    return '\0';
}

char JsonParser::get() {
    if (m_index < m_text.size()) {
        return m_text[m_index++];
    }
    return '\0';
}

bool JsonParser::eof() const {
    return m_index >= m_text.size();
}

bool JsonParser::parseValue(JsonValue& val) {
    skipWhitespace();
    if (eof()) return false;

    char c = peek();
    if (c == '{') return parseObject(val);
    if (c == '[') return parseArray(val);
    if (c == '"') {
        std::string s;
        if (!parseString(s)) return false;
        val = JsonValue(s);
        return true;
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        double d;
        if (!parseNumber(d)) return false;
        val = JsonValue(d);
        return true;
    }
    if (c == 't') return parseLiteral("true", val, JsonType::BOOLEAN);
    if (c == 'f') return parseLiteral("false", val, JsonType::BOOLEAN);
    if (c == 'n') return parseLiteral("null", val, JsonType::NUL);

    return false;
}

bool JsonParser::parseObject(JsonValue& val) {
    if (m_depth > 32) return false; // Prevent stack overflow from deep recursion
    ++m_depth;

    if (get() != '{') { --m_depth; return false; }

    val = JsonValue(JsonType::OBJECT);
    skipWhitespace();

    if (peek() == '}') {
        get();
        --m_depth;
        return true;
    }

    while (!eof()) {
        skipWhitespace();
        if (peek() != '"') { --m_depth; return false; }
        std::string key;
        if (!parseString(key)) { --m_depth; return false; }

        skipWhitespace();
        if (get() != ':') { --m_depth; return false; }

        JsonValue childVal;
        if (!parseValue(childVal)) { --m_depth; return false; }

        val.m_object[key] = std::move(childVal);

        skipWhitespace();
        char next = get();
        if (next == '}') {
            --m_depth;
            return true;
        }
        if (next != ',') {
            --m_depth;
            return false;
        }
    }
    --m_depth;
    return false;
}

bool JsonParser::parseArray(JsonValue& val) {
    if (m_depth > 32) return false;
    ++m_depth;

    if (get() != '[') { --m_depth; return false; }

    val = JsonValue(JsonType::ARRAY);
    skipWhitespace();

    if (peek() == ']') {
        get();
        --m_depth;
        return true;
    }

    while (!eof()) {
        JsonValue item;
        if (!parseValue(item)) { --m_depth; return false; }
        val.m_array.push_back(std::move(item));

        skipWhitespace();
        char next = get();
        if (next == ']') {
            --m_depth;
            return true;
        }
        if (next != ',') {
            --m_depth;
            return false;
        }
    }
    --m_depth;
    return false;
}

bool JsonParser::parseString(std::string& str) {
    if (get() != '"') return false;
    str.clear();

    while (!eof()) {
        char c = get();
        if (c == '"') {
            return true;
        }
        if (c == '\\') {
            if (eof()) return false;
            char esc = get();
            switch (esc) {
                case '"':  str += '"'; break;
                case '\\': str += '\\'; break;
                case '/':  str += '/'; break;
                case 'b':  str += '\b'; break;
                case 'f':  str += '\f'; break;
                case 'n':  str += '\n'; break;
                case 'r':  str += '\r'; break;
                case 't':  str += '\t'; break;
                case 'u': {
                    char hex[5] = {0};
                    for (int i = 0; i < 4 && !eof(); ++i) {
                        hex[i] = get();
                    }
                    unsigned int cp = static_cast<unsigned int>(std::strtoul(hex, nullptr, 16));

                    // Check for UTF-16 surrogate pairs (\uD800..\uDBFF followed by \uDC00..\uDFFF)
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (peek() == '\\') {
                            size_t savedIndex = m_index;
                            get(); // '\\'
                            if (peek() == 'u') {
                                get(); // 'u'
                                char hex2[5] = {0};
                                for (int i = 0; i < 4 && !eof(); ++i) {
                                    hex2[i] = get();
                                }
                                unsigned int low = static_cast<unsigned int>(std::strtoul(hex2, nullptr, 16));
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                } else {
                                    m_index = savedIndex;
                                }
                            } else {
                                m_index = savedIndex;
                            }
                        }
                    }

                    // Encode cp to UTF-8 multi-byte
                    if (cp <= 0x7F) {
                        str += static_cast<char>(cp);
                    } else if (cp <= 0x7FF) {
                        str += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                        str += static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp <= 0xFFFF) {
                        str += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
                        str += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        str += static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp <= 0x10FFFF) {
                        str += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                        str += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        str += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        str += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: str += esc; break;
            }
        } else {
            str += c;
        }
    }
    return false;
}

bool JsonParser::parseNumber(double& num) {
    size_t start = m_index;
    if (peek() == '-') get();

    while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
        get();
    }

    if (!eof() && peek() == '.') {
        get();
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            get();
        }
    }

    if (!eof() && (peek() == 'e' || peek() == 'E')) {
        get();
        if (!eof() && (peek() == '+' || peek() == '-')) get();
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            get();
        }
    }

    std::string numStr = m_text.substr(start, m_index - start);
    char* endPtr = nullptr;
    num = std::strtod(numStr.c_str(), &endPtr);
    return endPtr != numStr.c_str();
}

bool JsonParser::parseLiteral(const std::string& lit, JsonValue& val, JsonType type) {
    for (char c : lit) {
        if (get() != c) return false;
    }
    if (type == JsonType::BOOLEAN) {
        val = JsonValue(lit == "true");
    } else {
        val = JsonValue(JsonType::NUL);
    }
    return true;
}

} // namespace yt
