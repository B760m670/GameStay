#include "atlas.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <vector>

#include "../platform/platform.h"

namespace atlas {
namespace {
std::unordered_map<std::string, Sprite> sprites;
std::vector<gfx::Texture*> pages;
}

bool load(const std::string& dir) {
    std::vector<unsigned char> idx;
    if (!platform::readFile(dir + "/atlas.txt", idx)) {
        std::fprintf(stderr, "[atlas] no index in %s\n", dir.c_str());
        return false;
    }
    std::istringstream in(std::string(idx.begin(), idx.end()));
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string kind;
        l >> kind;
        if (kind == "page") {
            int n;
            std::string file;
            l >> n >> file;
            pages.resize(std::max<size_t>(pages.size(), n + 1));
            pages[n] = gfx::loadTexture(dir + "/" + file, true);
        } else if (kind == "sprite") {
            std::string name;
            int page;
            float x, y, w, h, px, py;
            l >> name >> page >> x >> y >> w >> h >> px >> py;
            if (page < 0 || page >= (int)pages.size() || !pages[page]) continue;
            const gfx::Texture* t = pages[page];
            Sprite s;
            s.tex = t;
            s.u0 = x / t->w;
            s.v0 = y / t->h;
            s.u1 = (x + w) / t->w;
            s.v1 = (y + h) / t->h;
            s.w = w;
            s.h = h;
            s.px = px;
            s.py = py;
            sprites[name] = s;
        }
    }
    std::fprintf(stderr, "[atlas] %zu sprites, %zu pages\n", sprites.size(), pages.size());
    return !sprites.empty();
}

const Sprite* get(const std::string& name) {
    auto it = sprites.find(name);
    return it == sprites.end() ? nullptr : &it->second;
}

void draw(const Sprite* s, float x, float y, float scale, uint32_t color, bool flipX) {
    if (!s) return;
    float left = flipX ? x - (s->w - s->px) * scale : x - s->px * scale;
    float top = y - s->py * scale;
    float u0 = flipX ? s->u1 : s->u0, u1 = flipX ? s->u0 : s->u1;
    gfx::rect(s->tex, left, top, s->w * scale, s->h * scale, u0, s->v0, u1, s->v1, color);
}

void drawRect(const Sprite* s, float x, float y, float w, float h, uint32_t color) {
    if (!s) return;
    gfx::rect(s->tex, x, y, w, h, s->u0, s->v0, s->u1, s->v1, color);
}

void draw9(const Sprite* s, float x, float y, float w, float h, float border, float db, uint32_t color) {
    if (!s) return;
    float bu = border / s->w * (s->u1 - s->u0), bv = border / s->h * (s->v1 - s->v0);
    db = std::min(db, std::min(w, h) / 2);
    float xs[4] = {x, x + db, x + w - db, x + w};
    float ys[4] = {y, y + db, y + h - db, y + h};
    float us[4] = {s->u0, s->u0 + bu, s->u1 - bu, s->u1};
    float vs[4] = {s->v0, s->v0 + bv, s->v1 - bv, s->v1};
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            gfx::rect(s->tex, xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], us[i], vs[j], us[i + 1], vs[j + 1], color);
}

}  // namespace atlas
