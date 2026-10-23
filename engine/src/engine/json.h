#pragma once
// Минимальный JSON: разбор ответов сервера и экранирование строк для запросов.
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace json {

struct Value {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Value> arr;
    std::map<std::string, Value> obj;

    const Value& operator[](const std::string& k) const;
    const Value& operator[](size_t i) const;
    size_t size() const { return kind == Array ? arr.size() : kind == Object ? obj.size() : 0; }
    long long i64(long long def = 0) const { return kind == Number ? (long long)num : def; }
    std::string s(const std::string& def = "") const { return kind == String ? str : def; }
    bool has(const std::string& k) const { return kind == Object && obj.count(k); }
};

// Разобрать текст; при ошибке возвращает Null.
Value parse(const std::string& text);
// Экранировать строку для вставки в JSON (с кавычками).
std::string quote(const std::string& s);

}  // namespace json
