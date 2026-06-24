#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cctype>

namespace quant {
namespace json_utils {

inline std::string escapeString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

class JsonWriter {
public:
    JsonWriter() : ss_() {}

    JsonWriter& startObject() {
        if (!stack_.empty() && stack_.back() == ARRAY && !array_first_.back()) {
            ss_ << ',';
        }
        ss_ << '{';
        stack_.push_back(OBJECT);
        obj_first_.push_back(true);
        return *this;
    }

    JsonWriter& endObject() {
        ss_ << '}';
        stack_.pop_back();
        obj_first_.pop_back();
        if (!stack_.empty() && stack_.back() == ARRAY) {
            array_first_.back() = false;
        }
        return *this;
    }

    JsonWriter& startArray() {
        if (!stack_.empty() && stack_.back() == ARRAY && !array_first_.back()) {
            ss_ << ',';
        }
        ss_ << '[';
        stack_.push_back(ARRAY);
        array_first_.push_back(true);
        return *this;
    }

    JsonWriter& endArray() {
        ss_ << ']';
        stack_.pop_back();
        array_first_.pop_back();
        if (!stack_.empty() && stack_.back() == ARRAY) {
            array_first_.back() = false;
        }
        return *this;
    }

    JsonWriter& key(const std::string& k) {
        if (!obj_first_.back()) {
            ss_ << ',';
        }
        ss_ << '"' << escapeString(k) << "\":";
        obj_first_.back() = false;
        return *this;
    }

    JsonWriter& value(const std::string& v) {
        if (!stack_.empty() && stack_.back() == ARRAY && !array_first_.back()) {
            ss_ << ',';
        }
        ss_ << '"' << escapeString(v) << '"';
        if (!stack_.empty() && stack_.back() == ARRAY) {
            array_first_.back() = false;
        }
        return *this;
    }

    JsonWriter& value(const char* v) {
        return value(std::string(v));
    }

    JsonWriter& value(double v) {
        if (!stack_.empty() && stack_.back() == ARRAY && !array_first_.back()) {
            ss_ << ',';
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%.6g", v);
        ss_ << buf;
        if (!stack_.empty() && stack_.back() == ARRAY) {
            array_first_.back() = false;
        }
        return *this;
    }

    JsonWriter& value(int v) {
        if (!stack_.empty() && stack_.back() == ARRAY && !array_first_.back()) {
            ss_ << ',';
        }
        ss_ << v;
        if (!stack_.empty() && stack_.back() == ARRAY) {
            array_first_.back() = false;
        }
        return *this;
    }

    JsonWriter& value(bool v) {
        if (!stack_.empty() && stack_.back() == ARRAY && !array_first_.back()) {
            ss_ << ',';
        }
        ss_ << (v ? "true" : "false");
        if (!stack_.empty() && stack_.back() == ARRAY) {
            array_first_.back() = false;
        }
        return *this;
    }

    JsonWriter& nullValue() {
        if (!stack_.empty() && stack_.back() == ARRAY && !array_first_.back()) {
            ss_ << ',';
        }
        ss_ << "null";
        if (!stack_.empty() && stack_.back() == ARRAY) {
            array_first_.back() = false;
        }
        return *this;
    }

    JsonWriter& keyValue(const std::string& k, const std::string& v) {
        return key(k).value(v);
    }

    JsonWriter& keyValue(const std::string& k, const char* v) {
        return key(k).value(std::string(v));
    }

    JsonWriter& keyValue(const std::string& k, double v) {
        return key(k).value(v);
    }

    JsonWriter& keyValue(const std::string& k, int v) {
        return key(k).value(v);
    }

    JsonWriter& keyValue(const std::string& k, bool v) {
        return key(k).value(v);
    }

    JsonWriter& keyNull(const std::string& k) {
        return key(k).nullValue();
    }

    JsonWriter& raw(const std::string& json) {
        ss_ << json;
        return *this;
    }

    std::string str() const { return ss_.str(); }

private:
    enum ContainerType { OBJECT, ARRAY };
    std::ostringstream ss_;
    std::vector<ContainerType> stack_;
    std::vector<bool> obj_first_;
    std::vector<bool> array_first_;
};

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    enum Type { NUL, BOOLEAN, NUMBER, STRING, ARRAY, OBJECT };
    Type type = NUL;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    JsonArray arr_val;
    JsonObject obj_val;
};

class JsonParser {
public:
    JsonParser(const std::string& s) : str_(s), pos_(0) {}

    bool parse(JsonValue& result) {
        skipWhitespace();
        if (!parseValue(result)) return false;
        return true;
    }

private:
    bool parseValue(JsonValue& v) {
        skipWhitespace();
        if (pos_ >= str_.size()) return false;

        char c = str_[pos_];
        if (c == '{') return parseObject(v);
        if (c == '[') return parseArray(v);
        if (c == '"') return parseString(v);
        if (c == 't' || c == 'f') return parseBool(v);
        if (c == 'n') return parseNull(v);
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(v);
        return false;
    }

    bool parseObject(JsonValue& v) {
        v.type = JsonValue::OBJECT;
        v.obj_val.clear();
        pos_++;
        skipWhitespace();
        if (pos_ < str_.size() && str_[pos_] == '}') {
            pos_++;
            return true;
        }
        while (pos_ < str_.size()) {
            skipWhitespace();
            if (pos_ >= str_.size() || str_[pos_] != '"') return false;
            std::string key;
            if (!parseStringRaw(key)) return false;
            skipWhitespace();
            if (pos_ >= str_.size() || str_[pos_] != ':') return false;
            pos_++;
            JsonValue val;
            if (!parseValue(val)) return false;
            v.obj_val[key] = val;
            skipWhitespace();
            if (pos_ < str_.size() && str_[pos_] == ',') {
                pos_++;
                continue;
            }
            if (pos_ < str_.size() && str_[pos_] == '}') {
                pos_++;
                return true;
            }
            return false;
        }
        return false;
    }

    bool parseArray(JsonValue& v) {
        v.type = JsonValue::ARRAY;
        v.arr_val.clear();
        pos_++;
        skipWhitespace();
        if (pos_ < str_.size() && str_[pos_] == ']') {
            pos_++;
            return true;
        }
        while (pos_ < str_.size()) {
            JsonValue val;
            if (!parseValue(val)) return false;
            v.arr_val.push_back(val);
            skipWhitespace();
            if (pos_ < str_.size() && str_[pos_] == ',') {
                pos_++;
                continue;
            }
            if (pos_ < str_.size() && str_[pos_] == ']') {
                pos_++;
                return true;
            }
            return false;
        }
        return false;
    }

    bool parseString(JsonValue& v) {
        v.type = JsonValue::STRING;
        return parseStringRaw(v.str_val);
    }

    bool parseStringRaw(std::string& out) {
        if (pos_ >= str_.size() || str_[pos_] != '"') return false;
        pos_++;
        out.clear();
        while (pos_ < str_.size()) {
            char c = str_[pos_];
            if (c == '"') {
                pos_++;
                return true;
            }
            if (c == '\\' && pos_ + 1 < str_.size()) {
                pos_++;
                char e = str_[pos_];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    default: out += e; break;
                }
                pos_++;
            } else {
                out += c;
                pos_++;
            }
        }
        return false;
    }

    bool parseNumber(JsonValue& v) {
        v.type = JsonValue::NUMBER;
        size_t start = pos_;
        if (pos_ < str_.size() && str_[pos_] == '-') pos_++;
        while (pos_ < str_.size() && std::isdigit((unsigned char)str_[pos_])) pos_++;
        if (pos_ < str_.size() && str_[pos_] == '.') {
            pos_++;
            while (pos_ < str_.size() && std::isdigit((unsigned char)str_[pos_])) pos_++;
        }
        if (pos_ < str_.size() && (str_[pos_] == 'e' || str_[pos_] == 'E')) {
            pos_++;
            if (pos_ < str_.size() && (str_[pos_] == '+' || str_[pos_] == '-')) pos_++;
            while (pos_ < str_.size() && std::isdigit((unsigned char)str_[pos_])) pos_++;
        }
        v.num_val = std::stod(str_.substr(start, pos_ - start));
        return true;
    }

    bool parseBool(JsonValue& v) {
        v.type = JsonValue::BOOLEAN;
        if (str_.substr(pos_, 4) == "true") {
            v.bool_val = true;
            pos_ += 4;
            return true;
        }
        if (str_.substr(pos_, 5) == "false") {
            v.bool_val = false;
            pos_ += 5;
            return true;
        }
        return false;
    }

    bool parseNull(JsonValue& v) {
        v.type = JsonValue::NUL;
        if (str_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return true;
        }
        return false;
    }

    void skipWhitespace() {
        while (pos_ < str_.size() && std::isspace((unsigned char)str_[pos_])) {
            pos_++;
        }
    }

    const std::string& str_;
    size_t pos_;
};

}
}
