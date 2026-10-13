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
}
