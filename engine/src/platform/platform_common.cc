#include "platform.h"
#include <chrono>
#include <cstdio>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace platform {
bool readFile(const std::string& path, std::vector<unsigned char>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    size_t got = n > 0 ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == out.size();
}

bool writeFile(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (std::fclose(f) == 0) && ok;
    ok = ok && std::rename(tmp.c_str(), path.c_str()) == 0;
#ifdef __EMSCRIPTEN__
    // браузер: папка сохранений смонтирована в IndexedDB — сбрасываем изменения туда
    // одна синхронизация за раз; запись во время неё — ещё один проход после
    if (ok) EM_ASM({
        if (Module.lfSync) { Module.lfSyncAgain = true; return; }
        Module.lfSync = true;
        (function go() {
            FS.syncfs(false, function(err) {
                if (err) console.warn('save sync', err);
                if (Module.lfSyncAgain) { Module.lfSyncAgain = false; go(); } else Module.lfSync = false;
            });
        })();
    });
#endif
    return ok;
}

long long nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
}
