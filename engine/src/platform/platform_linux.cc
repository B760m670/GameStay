// Linux: используется для разработки и автотестов (скриншоты под Xvfb).
#include "platform.h"
#include <cstdlib>
#include <sys/stat.h>

namespace platform {
std::string assetDir() {
    const char* env = std::getenv("LANTERNFALL_ASSETS");
    return env ? env : "assets";
}
std::string saveDir() {
    const char* env = std::getenv("LANTERNFALL_SAVE_DIR");
    std::string dir = env ? env : ".";
    mkdir(dir.c_str(), 0755);
    return dir;
}
}
