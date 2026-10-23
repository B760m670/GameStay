#include "json.h"

#include <cstdio>
#include <cstdlib>

namespace json {
namespace {
const Value kNull;

struct Parser {
    const std::string& t;
    size_t i = 0;
    bool ok = true;
    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) i++; }
    bool eat(char c) { ws(); if (i < t.size() && t[i] == c) { i++; return true; } return false; }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    std::string string() {
        std::string out;
        if (!eat('"')) { ok = false; return out; }
        while (i < t.size() && t[i] != '"') {
            char c = t[i++];
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) break;
            char e = t[i++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp = (unsigned)std::strtoul(t.substr(i, 4).c_str(), nullptr, 16);
                    i += 4;
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                        unsigned lo = (unsigned)std::strtoul(t.substr(i + 2, 4).c_str(), nullptr, 16);
                        i += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(out, cp);
                    break;
                }
                default: out += e;
            }
        }
        i++;
        return out;
    }
    Value value(int depth = 0) {
        Value v;
        ws();
        if (i >= t.size() || depth > 64) { ok = false; return v; }
        char c = t[i];
        if (c == '{') {
            i++;
            v.kind = Value::Object;
            if (eat('}')) return v;
            do {
                std::string k = string();
                if (!eat(':')) { ok = false; return v; }
                v.obj[k] = value(depth + 1);
            } while (ok && eat(','));
            if (!eat('}')) ok = false;
        } else if (c == '[') {
            i++;
            v.kind = Value::Array;
            if (eat(']')) return v;
            do v.arr.push_back(value(depth + 1)); while (ok && eat(','));
            if (!eat(']')) ok = false;
        } else if (c == '"') {
            v.kind = Value::String;
            v.str = string();
        } else if (t.compare(i, 4, "true") == 0) { v.kind = Value::Bool; v.b = true; i += 4; }
        else if (t.compare(i, 5, "false") == 0) { v.kind = Value::Bool; i += 5; }
        else if (t.compare(i, 4, "null") == 0) { i += 4; }
        else {
            char* end = nullptr;
            v.num = std::strtod(t.c_str() + i, &end);
            if (end == t.c_str() + i) { ok = false; return v; }
            v.kind = Value::Number;
            i = end - t.c_str();
        }
        return v;
    }
};
}  // namespace

const Value& Value::operator[](const std::string& k) const {
    if (kind != Object) return kNull;
    auto it = obj.find(k);
    return it == obj.end() ? kNull : it->second;
}
const Value& Value::operator[](size_t i) const { return kind == Array && i < arr.size() ? arr[i] : kNull; }

Value parse(const std::string& text) {
    Parser p{text};
    Value v = p.value();
    return p.ok ? v : Value{};
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += (char)c;
        }
    }
    return out + "\"";
}

}  // namespace json
