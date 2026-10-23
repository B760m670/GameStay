// Браузер (Emscripten, WebGL2): ресурсы — из предзагруженных пакетов в /assets, сохранения — /save в IndexedDB
// (монтируется в web/pre.js до запуска). Сеть пока офлайн: игра работает с ИИ-базами и кампанией.
#include "platform.h"

#include <sys/stat.h>

#include "engine/net.h"

namespace platform {
std::string assetDir() { return "/assets"; }
std::string saveDir() {
    mkdir("/save", 0755);
    return "/save";
}
void httpRequest(int id, const std::string&, const std::string&, const std::string&, const std::string&) { net::deliver(id, 0, ""); }
void wsOpen(const std::string&) { net::wsDeliver(2, ""); }
void wsSend(const std::string&) {}
void wsClose() {}
void showKeyboard(bool) {}
}  // namespace platform
