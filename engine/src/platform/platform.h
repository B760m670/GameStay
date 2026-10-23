#pragma once
#include <string>
#include <vector>

namespace platform {
// Папка с ресурсами игры (внутри .app на iOS).
std::string assetDir();
// Папка для сохранений (Documents на iOS).
std::string saveDir();
bool readFile(const std::string& path, std::vector<unsigned char>& out);
bool writeFile(const std::string& path, const std::string& data);
// Unix-время в миллисекундах.
long long nowMs();
// Сеть (асинхронно; результат — через net::deliver / net::wsDeliver).
void httpRequest(int id, const std::string& method, const std::string& url, const std::string& body, const std::string& token);
void wsOpen(const std::string& url);
void wsSend(const std::string& text);
void wsClose();
// Экранная клавиатура (iOS); на компьютере ввод идёт с клавиатуры всегда.
void showKeyboard(bool show);
}
