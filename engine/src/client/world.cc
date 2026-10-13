#include "world.h"

#include <algorithm>
#include <cmath>

#include "engine/atlas.h"
#include "engine/font.h"
#include "engine/gfx.h"

namespace client::world {
using gfx::rgba;

namespace {
constexpr float kSqrt2 = 1.41421356f;

struct UnitAnims { const char* move; const char* attack; };
UnitAnims animsFor(lf::TroopType t) {
    switch (t) {
        case lf::TroopType::Torch: return {"walk", "attack-melee-right"};
        case lf::TroopType::Archer: return {"walk", "holding-right-shoot"};
        case lf::TroopType::Shield: return {"walk", "attack-melee-left"};
        case lf::TroopType::Sapper: return {"sprint", "interact-right"};
        case lf::TroopType::Shade: return {"sprint", "attack-melee-right"};
        case lf::TroopType::Owl: return {"fall", "holding-both-shoot"};
        case lf::TroopType::Morok: return {"walk", "attack-melee-right"};
        case lf::TroopType::Brute: return {"walk", "attack-melee-right"};
        case lf::TroopType::Leech: return {"sprint", "attack-melee-left"};
        case lf::TroopType::Wraith: return {"fall", "holding-both-shoot"};
        default: return {"walk", "walk"};
    }
}

uint32_t hash2(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float smoothstep(float a, float b, float x) {
    float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

// Квад произвольной формы с одним цветом.
void line(const Vec2& a, const Vec2& b, float w, uint32_t c) {
    float dx = b.x - a.x, dy = b.y - a.y, l = std::hypot(dx, dy);
    if (l < 0.01f) return;
    float nx = -dy / l * w / 2, ny = dx / l * w / 2;
    gfx::Vtx v[4] = {{a.x + nx, a.y + ny, 0, 0, c}, {b.x + nx, b.y + ny, 1, 0, c}, {b.x - nx, b.y - ny, 1, 1, c}, {a.x - nx, a.y - ny, 0, 1, c}};
    gfx::quad(gfx::white(), v);
}

void glowAt(float x, float y, float r, uint32_t c) {
    const atlas::Sprite* g = atlas::get("fx/glow");
    if (!g) return;
    gfx::setBlend(gfx::Blend::Add);
    atlas::drawRect(g, x - r, y - r, r * 2, r * 2, c);
    gfx::setBlend(gfx::Blend::Alpha);
}

void ellipseSprite(const char* name, float x, float y, float rx, float ry, uint32_t c, bool add = false) {
    const atlas::Sprite* s = atlas::get(name);
    if (!s) return;
    if (add) gfx::setBlend(gfx::Blend::Add);
    atlas::drawRect(s, x - rx, y - ry, rx * 2, ry * 2, c);
    if (add) gfx::setBlend(gfx::Blend::Alpha);
}
}  // namespace

// Облик постройки по уровню: 3 ступени (t1..t3) на всю шкалу уровней.
const atlas::Sprite* buildingSprite(lf::BType type, int level) {
    const auto& def = lf::B(type);
    int tier = std::clamp((level - 1) * 3 / std::max(1, def.maxLevel) + 1, 1, 3);
    for (int t = tier; t >= 1; t--)
        if (const atlas::Sprite* s = atlas::get(std::string("b/") + def.id + "_t" + std::to_string(t))) return s;
    return atlas::get(std::string("b/") + def.id);
}

void drawGround(const Camera& cam) {
    const float s = cam.zoom * 0.5f;
    std::string name;
    for (int j = 0; j < lf::kGrid; j++)
        for (int i = 0; i < lf::kGrid; i++) {
            Vec2 p = cam.toScreen(i + 0.5f, j + 0.5f);
            if (p.x < -kTW * cam.zoom || p.x > cam.w + kTW * cam.zoom || p.y < -kTH * cam.zoom || p.y > cam.h + kTH * cam.zoom) continue;
            bool edge = i < lf::kBorder || j < lf::kBorder || i >= lf::kGrid - lf::kBorder || j >= lf::kGrid - lf::kBorder;
            uint32_t h = hash2(i, j);
            name = edge ? "g/edge" + std::to_string(h % 3) : "g/grass" + std::to_string(h % 6);
            atlas::draw(atlas::get(name), p.x, p.y, s);
        }
}

void drawFog(const Camera& cam, const std::vector<lf::LightSource>& src, const std::vector<lf::Eclipse>& ecl, float t, float darkness,
             bool forbidTint) {
    constexpr int N = lf::kGrid + 1;
    static float L[N * N];
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            float best = 0;
            for (const auto& s : src) {
                float d = std::hypot(i - s.x, j - s.y);
                best = std::max(best, 1.f - smoothstep(s.r - 1.4f, s.r + 0.7f, d));
            }
            for (const auto& e : ecl) {
                float d = std::hypot(i - e.x, j - e.y);
                best *= smoothstep(e.r - 1.2f, e.r + 0.3f, d);
            }
            L[j * N + i] = best;
        }
    const uint32_t fog = rgba(14, 9, 30), red = rgba(150, 20, 30);
    for (int j = 0; j < lf::kGrid; j++)
        for (int i = 0; i < lf::kGrid; i++) {
            Vec2 a = cam.toScreen((float)i, (float)j), b = cam.toScreen(i + 1.f, (float)j), c = cam.toScreen(i + 1.f, j + 1.f), d = cam.toScreen((float)i, j + 1.f);
            if (b.x < 0 || d.x > cam.w || a.y > cam.h || c.y < 0) continue;
            float la = L[j * N + i], lb = L[j * N + i + 1], lc = L[(j + 1) * N + i + 1], ld = L[(j + 1) * N + i];
            if (la > 0.999f && lb > 0.999f && lc > 0.999f && ld > 0.999f && !forbidTint) continue;
            auto col = [&](float l) {
                if (forbidTint && l > 0.5f) return gfx::withAlpha(red, 0.22f * l);
                return gfx::withAlpha(fog, darkness * (1 - l));
            };
            gfx::Vtx v[4] = {{a.x, a.y, 0, 0, col(la)}, {b.x, b.y, 0, 0, col(lb)}, {c.x, c.y, 0, 0, col(lc)}, {d.x, d.y, 0, 0, col(ld)}};
            gfx::quad(gfx::white(), v);
        }
    // тёплое свечение света
    for (const auto& s : src) {
        Vec2 p = cam.toScreen(s.x, s.y);
        float flick = 1 + std::sin(t * 3 + s.x) * 0.015f;
        float rx = s.r * kTW * cam.zoom / kSqrt2 * flick;
        ellipseSprite("fx/glow", p.x, p.y, rx, rx / 2, rgba(255, 170, 70, 46), true);
    }
}

void drawFootprint(const Camera& cam, float x, float y, float size, uint32_t color) {
    Vec2 a = cam.toScreen(x, y), b = cam.toScreen(x + size, y), c = cam.toScreen(x + size, y + size), d = cam.toScreen(x, y + size);
    gfx::Vtx v[4] = {{a.x, a.y, 0, 0, color}, {b.x, b.y, 0, 0, color}, {c.x, c.y, 0, 0, color}, {d.x, d.y, 0, 0, color}};
    gfx::quad(gfx::white(), v);
}

void drawBarAt(const Camera& cam, float wx, float wy, float z, float frac, uint32_t color, float width) {
    Vec2 p = cam.toScreen(wx, wy, z);
    float w = width * std::max(0.6f, cam.zoom), h = 6;
    gfx::fillRect(p.x - w / 2 - 1.5f, p.y - 1.5f, w + 3, h + 3, rgba(10, 6, 18, 200));
    gfx::fillRect(p.x - w / 2, p.y, w * std::clamp(frac, 0.f, 1.f), h, color);
}

void drawBuilding(const Camera& cam, lf::BType type, float x, float y, int level, const BuildingLook& look, float t) {
    const auto& def = lf::B(type);
    float s = (float)def.size;
    Vec2 c = cam.toScreen(x + s / 2, y + s / 2);
    if (look.ghost) drawFootprint(cam, x, y, s, look.ghost > 0 ? rgba(90, 255, 120, 90) : rgba(255, 60, 60, 110));
    if (look.selected) {
        float pulse = 0.5f + 0.5f * std::sin(t * 5);
        drawFootprint(cam, x - 0.15f, y - 0.15f, s + 0.3f, rgba(255, 230, 120, (int)(50 + 50 * pulse)));
    }
    if (look.dead) {
        drawFootprint(cam, x + 0.1f, y + 0.1f, s - 0.2f, rgba(30, 24, 30, 150));
        uint32_t h = hash2((int)x * 7, (int)y * 13);
        for (int k = 0; k < (int)s * 3; k++) {
            float px = x + 0.3f + ((h >> (k % 16)) & 7) / 7.f * (s - 0.6f);
            float py = y + 0.3f + ((h >> ((k * 3) % 16)) & 7) / 7.f * (s - 0.6f);
            Vec2 p = cam.toScreen(px, py, 3);
            ellipseSprite("fx/soft", p.x, p.y, 7 * cam.zoom, 4 * cam.zoom, k % 2 ? rgba(70, 64, 72, 230) : rgba(90, 70, 56, 230));
        }
        return;
    }
    const atlas::Sprite* sp = buildingSprite(type, level);
    float dim = look.dim * (look.constructing ? 0.7f : 1.f);
    uint32_t col = rgba((int)(255 * dim), (int)(255 * dim), (int)(255 * std::min(1.f, dim * 1.08f)), (int)(255 * look.alpha * (look.constructing ? 0.75f : 1)));
    if (sp) {
        atlas::draw(sp, c.x, c.y, cam.zoom * 0.5f, col);
    } else {
        drawFootprint(cam, x + 0.1f, y + 0.1f, s - 0.2f, rgba(140, 120, 100, 220));
    }
    if (look.hitFlash > 0 && sp) {
        gfx::setBlend(gfx::Blend::Add);
        atlas::draw(sp, c.x, c.y, cam.zoom * 0.5f, rgba(255, 255, 255, (int)(look.hitFlash * 500)));
        gfx::setBlend(gfx::Blend::Alpha);
    }
    if (look.unpowered) {
        Vec2 p = cam.toScreen(x + s / 2, y + s / 2, 70 + s * 6);
        float r = 11 * std::max(0.7f, cam.zoom);
        atlas::drawRect(atlas::get("icon/eclipse"), p.x - r, p.y - r, r * 2, r * 2);
    }
}

void drawTroop(const Camera& cam, const lf::BTroop& tr, float t) {
    float lift = tr.air ? 46 + std::sin(t * 4 + tr.bob) * 4 : 0;
    Vec2 g = cam.toScreen(tr.x, tr.y);
    Vec2 p = cam.toScreen(tr.x, tr.y, lift);
    float alpha = tr.hidden ? 0.4f : 1.f;
    if (tr.air) ellipseSprite("fx/shadow", g.x, g.y, 12 * cam.zoom, 6 * cam.zoom, rgba(0, 0, 0, 90));
    auto an = animsFor(tr.type);
    // направление спрайта: d=0 смотрит в мировой угол 45°, далее +45° на шаг
    float deg = tr.angle * 57.2958f;
    int dir = ((int)std::lround((deg - 45.f) / 45.f) % 8 + 8) % 8;
    std::string name;
    if (tr.attacking) {
        int f = tr.def->cd > 0 ? (tr.cd > tr.def->cd * 0.5f ? 1 : 0) : 0;
        name = std::string("u/") + tr.def->id + "/" + an.attack + "_" + std::to_string(dir) + "_" + std::to_string(f);
    } else {
        int f = (int)(tr.animT * 8 * tr.def->speed / 1.5f + tr.bob) % 4;
        name = std::string("u/") + tr.def->id + "/" + an.move + "_" + std::to_string(dir) + "_" + std::to_string(f);
    }
    const atlas::Sprite* sp = atlas::get(name);
    uint32_t col = rgba(255, 255, 255, (int)(255 * alpha));
    if (tr.def->haze) {
        glowAt(p.x, p.y - 14 * cam.zoom, 22 * cam.zoom, rgba(150, 90, 220, (int)(60 * alpha)));
        col = rgba(190, 170, 230, (int)(235 * alpha));
    }
    if (sp) atlas::draw(sp, p.x, p.y, cam.zoom * 0.5f, col);
    else ellipseSprite("fx/soft", p.x, p.y - 8 * cam.zoom, 8 * cam.zoom, 10 * cam.zoom, tr.def->color);
    if (tr.hitT > 0 && sp) {
        gfx::setBlend(gfx::Blend::Add);
        atlas::draw(sp, p.x, p.y, cam.zoom * 0.5f, rgba(255, 120, 120, 160));
        gfx::setBlend(gfx::Blend::Alpha);
    }
    if (tr.hp < tr.maxHp) {
        float w = 22 * std::max(0.7f, cam.zoom);
        float y = p.y - 44 * cam.zoom;
        gfx::fillRect(p.x - w / 2 - 1, y - 1, w + 2, 6, rgba(10, 6, 18, 200));
        gfx::fillRect(p.x - w / 2, y, w * std::max(0.f, tr.hp / tr.maxHp), 4, tr.def->haze ? rgba(200, 110, 255) : rgba(130, 220, 80));
    }
}

void drawProjectile(const Camera& cam, const lf::Projectile& p) {
    float h0 = p.fromAir ? 46.f : (p.src ? 40.f : 14.f);
    float z = h0 * (1 - p.k) + 12 * p.k;
    if (p.kind == lf::Proj::Rock) z += std::sin(p.k * 3.14159f) * 110;
    Vec2 s = cam.toScreen(p.x, p.y, z);
    switch (p.kind) {
        case lf::Proj::Rock: {
            Vec2 g = cam.toScreen(p.x, p.y);
            ellipseSprite("fx/shadow", g.x, g.y, 7 * cam.zoom, 3.5f * cam.zoom, rgba(0, 0, 0, 90));
            ellipseSprite("fx/dot", s.x, s.y, 7 * cam.zoom, 7 * cam.zoom, rgba(150, 140, 140));
            break;
        }
        case lf::Proj::Bolt:
        case lf::Proj::Arrow: {
            Vec2 a = cam.toScreen(p.sx, p.sy), b = cam.toScreen(p.tx, p.ty);
            float dx = b.x - a.x, dy = b.y - a.y, l = std::hypot(dx, dy) + 0.001f;
            float L = (p.kind == lf::Proj::Bolt ? 16 : 9) * cam.zoom;
            line({s.x - dx / l * L, s.y - dy / l * L}, s, std::max(1.5f, 2.5f * cam.zoom), p.kind == lf::Proj::Bolt ? rgba(240, 225, 190) : rgba(210, 240, 170));
            break;
        }
        default:
            glowAt(s.x, s.y, 9 * cam.zoom + 3, p.kind == lf::Proj::Wisp ? rgba(200, 140, 255, 230) : rgba(255, 240, 200, 230));
    }
}

void drawBeam(const Camera& cam, const lf::BBuilding& b, const lf::BTroop& tr) {
    Vec2 a = cam.toScreen(b.cx, b.cy, 64), c = cam.toScreen(tr.x, tr.y, tr.air ? 46.f : 10.f);
    float k = std::min(1.f, b.beamT / 1.5f);
    gfx::setBlend(gfx::Blend::Add);
    line(a, c, (3 + k * 6) * cam.zoom + 1, rgba(255, 110, 230, (int)(110 + k * 100)));
    line(a, c, 1.5f, rgba(255, 255, 255, 230));
    gfx::setBlend(gfx::Blend::Alpha);
    glowAt(c.x, c.y, 14 * cam.zoom + 4, rgba(255, 150, 240, 200));
}

void drawEffect(const Camera& cam, const lf::Effect& e, float t) {
    float k = 1 - e.life / e.max;
    Vec2 p = cam.toScreen(e.x, e.y, e.air ? 46.f : 0.f);
    float z = cam.zoom;
    switch (e.kind) {
        case lf::Fx::Boom: {
            float r = std::max(1.f, e.r) * 22 * z * (0.4f + k);
            glowAt(p.x, p.y - 12 * z, r * 1.6f, rgba(255, 150, 60, (int)(255 * (1 - k))));
            for (int i = 0; i < 6; i++) {
                float a = i * 1.05f;
                ellipseSprite("fx/soft", p.x + std::cos(a) * r * 0.7f, p.y - 12 * z - k * 34 * z + std::sin(a) * r * 0.35f, r * 0.45f, r * 0.45f,
                              rgba(80, 70, 80, (int)(170 * (1 - k))));
            }
            break;
        }
        case lf::Fx::Blast: {
            float rx = std::max(0.5f, e.r) * kTW * z / kSqrt2 * (0.3f + k * 0.7f);
            ellipseSprite("fx/ring", p.x, p.y, rx, rx / 2, rgba(255, 210, 130, (int)(255 * (1 - k))));
            glowAt(p.x, p.y, rx * 0.6f, rgba(255, 170, 80, (int)(200 * (1 - k))));
            break;
        }
        case lf::Fx::Hit:
            glowAt(p.x, p.y - 10 * z, 8 * z + 2, rgba(255, 240, 200, (int)(255 * (1 - k))));
            break;
        case lf::Fx::Poof:
            ellipseSprite("fx/soft", p.x, p.y - 8 * z - k * 14 * z, (5 + k * 10) * z, (5 + k * 10) * z, rgba(160, 150, 170, (int)(150 * (1 - k))));
            break;
        case lf::Fx::Flame: {
            Vec2 a = cam.toScreen(e.sx, e.sy, 40);
            for (int i = 0; i <= 6; i++) {
                float f = i / 6.f;
                glowAt(a.x + (p.x - a.x) * f, a.y + (p.y - 8 * z - a.y) * f, (7 + f * 14) * z + 2, rgba(255, (int)(150 - f * 60), 40, (int)(230 * (1 - k))));
            }
            break;
        }
        case lf::Fx::EclipseFx: {
            float rx = lf::kEclipseRadius * kTW * z / kSqrt2 * (0.2f + k);
            ellipseSprite("fx/ring", p.x, p.y, rx, rx / 2, rgba(190, 140, 255, (int)(255 * (1 - k))));
            break;
        }
        case lf::Fx::Rift:
            for (int i = 0; i < 3; i++) {
                float rx = 40 * z * (0.4f + i * 0.3f + k * 0.3f);
                ellipseSprite("fx/ring", p.x, p.y, rx, rx / 2, rgba(170, 110, 240, (int)(230 * (1 - k))));
            }
            break;
    }
    (void)t;
}

void drawBuildingThumb(lf::BType type, float cx, float cy, float maxW, float maxH, int level) {
    const atlas::Sprite* sp = buildingSprite(type, level);
    if (!sp) return;
    float k = std::min(maxW / sp->w, maxH / sp->h);
    atlas::drawRect(sp, cx - sp->w * k / 2, cy - sp->h * k / 2, sp->w * k, sp->h * k);
}

void drawTroopThumb(lf::TroopType type, float cx, float cy, float size) {
    auto an = animsFor(type);
    const atlas::Sprite* sp = atlas::get(std::string("u/") + lf::T(type).id + "/" + an.move + "_0_0");
    if (!sp) return;
    float k = size / std::max(sp->w, sp->h);
    atlas::drawRect(sp, cx - sp->w * k / 2, cy - sp->h * k / 2, sp->w * k, sp->h * k);
}

}  // namespace client::world
