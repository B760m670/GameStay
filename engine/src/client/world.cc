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

// Свет на карте (кэш последнего drawFog): 0 — тьма, 1 — полный свет.
constexpr int kLM = lf::kGrid + 2 * kPad + 1;
float gLight[kLM * kLM];
float gDark = 0.74f;
bool gLightValid = false;

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

namespace {
// Поворотная часть (Арбалет, Катапульта): спрайты _d0.._d7 — поворот на 45° против часовой (вид сверху в Blender).
// kTurretBase — угол в Blender, куда смотрит _d0; мировой угол a соответствует углу -a в Blender.
constexpr float kTurretBase = 0.f;
const atlas::Sprite* turretSprite(lf::BType type, int level, float angle) {
    const auto& def = lf::B(type);
    int tier = std::clamp((level - 1) * 3 / std::max(1, def.maxLevel) + 1, 1, 3);
    float deg = -angle * 57.2958f - kTurretBase;
    int d = ((int)std::lround(deg / 45.f) % 8 + 8) % 8;
    return atlas::get(std::string("b/") + def.id + "_t" + std::to_string(tier) + "_d" + std::to_string(d));
}
}  // namespace

void drawGround(const Camera& cam) {
    const float s = cam.zoom * 0.5f;
    std::string name;
    for (int j = -kPad; j < lf::kGrid + kPad; j++)
        for (int i = -kPad; i < lf::kGrid + kPad; i++) {
            Vec2 p = cam.toScreen(i + 0.5f, j + 0.5f);
            if (p.x < -kTW * cam.zoom || p.x > cam.w + kTW * cam.zoom || p.y < -kTH * cam.zoom || p.y > cam.h + kTH * cam.zoom) continue;
            bool edge = i < lf::kBorder || j < lf::kBorder || i >= lf::kGrid - lf::kBorder || j >= lf::kGrid - lf::kBorder;
            uint32_t h = hash2(i + 100, j + 100);
            name = edge ? "g/edge" + std::to_string(h % 3) : "g/grass" + std::to_string(h % 6);
            atlas::draw(atlas::get(name), p.x, p.y, s);
        }
}

void drawDirt(const Camera& cam, float x, float y, float size) {
    Vec2 c = cam.toScreen(x + size / 2, y + size / 2);
    float rx = (size + 0.9f) * kTW * cam.zoom / 2, ry = rx / 2;
    ellipseSprite("fx/soft", c.x, c.y, rx * 1.05f, ry * 1.05f, rgba(92, 70, 44, 120));
    ellipseSprite("fx/soft", c.x, c.y, rx * 0.8f, ry * 0.8f, rgba(110, 84, 52, 110));
}

namespace {
struct Detail { const atlas::Sprite* sp; float x, y; };
std::vector<Detail>& meadow() {
    static std::vector<Detail> v;
    static bool built = false;
    if (built) return v;
    built = true;
    static const char* kinds[] = {"b/decor_grass", "b/decor_grass2", "b/decor_grass3", "b/decor_grass", "b/decor_fl_purple", "b/decor_fl_red",
                                  "b/decor_fl_yellow", "b/decor_pebble", "b/decor_pebble2", "b/decor_shroom", "b/decor_bush_small"};
    for (int j = lf::kBorder; j < lf::kGrid - lf::kBorder; j++)
        for (int i = lf::kBorder; i < lf::kGrid - lf::kBorder; i++) {
            uint32_t h = hash2(i * 3 + 7, j * 5 + 11);
            if (h % 100 >= 16) continue;
            const atlas::Sprite* sp = atlas::get(kinds[(h >> 8) % 11]);
            if (!sp) continue;
            v.push_back({sp, i + 0.2f + ((h >> 12) & 15) / 25.f, j + 0.2f + ((h >> 16) & 15) / 25.f});
        }
    return v;
}
}  // namespace

void drawMeadow(const Camera& cam) {
    for (const auto& d : meadow()) {
        Vec2 p = cam.toScreen(d.x, d.y);
        if (p.x < -40 || p.x > cam.w + 40 || p.y < -40 || p.y > cam.h + 40) continue;
        atlas::draw(d.sp, p.x, p.y, cam.zoom * 0.5f);
    }
}

const std::vector<Decor>& decor() {
    static std::vector<Decor> v;
    static bool built = false;
    if (built) return v;
    built = true;
    static const char* trees[] = {"b/decor_oak", "b/decor_oak_dark", "b/decor_fat", "b/decor_detailed", "b/decor_pine", "b/decor_pine2", "b/decor_cone", "b/decor_tall"};
    static const char* small[] = {"b/decor_rock_tall", "b/decor_rock_small", "b/decor_bush", "b/decor_bush2", "b/decor_stump", "b/decor_mushrooms", "b/decor_logs"};
    std::vector<uint8_t> used((lf::kGrid + 2 * kPad) * (lf::kGrid + 2 * kPad), 0);
    auto idx = [&](int i, int j) { return (j + kPad) * (lf::kGrid + 2 * kPad) + (i + kPad); };
    for (int j = -kPad; j < lf::kGrid + kPad - 1; j++)
        for (int i = -kPad; i < lf::kGrid + kPad - 1; i++) {
            bool outside = i < 0 || j < 0 || i >= lf::kGrid || j >= lf::kGrid;
            if (!outside) continue;
            uint32_t h = hash2(i * 13 + 5, j * 17 + 3);
            int dist = std::max({-i, -j, i - lf::kGrid + 1, j - lf::kGrid + 1});  // насколько далеко за краем
            int chance = dist <= 1 ? 22 : 45;
            if ((int)(h % 100) >= chance) continue;
            bool big = (h >> 9) % 100 < 60 && i + 1 < lf::kGrid + kPad && j + 1 < lf::kGrid + kPad;
            int sz = big ? 2 : 1;
            bool freeCells = true;
            for (int dy = 0; dy < sz; dy++)
                for (int dx = 0; dx < sz; dx++)
                    if (used[idx(i + dx, j + dy)]) freeCells = false;
            if (!freeCells) continue;
            for (int dy = 0; dy < sz; dy++)
                for (int dx = 0; dx < sz; dx++) used[idx(i + dx, j + dy)] = 1;
            const char* name = big ? ((h >> 13) % 6 == 0 ? "b/decor_rock_large" : trees[(h >> 15) % 8]) : small[(h >> 15) % 7];
            v.push_back({name, (float)i, (float)j, sz, i + j + sz * 1.f});
        }
    return v;
}

void drawDecor(const Camera& cam, const Decor& d) {
    Vec2 c = cam.toScreen(d.x + d.size / 2.f, d.y + d.size / 2.f);
    if (c.x < -200 || c.x > cam.w + 200 || c.y < -60 || c.y > cam.h + 300) return;
    // всё за краем поля — во Мгле: тёмные холодные силуэты
    atlas::draw(atlas::get(d.sprite), c.x, c.y, cam.zoom * 0.5f, rgba(70, 66, 104));
}

void drawFog(const Camera& cam, const std::vector<lf::LightSource>& src, const std::vector<lf::Eclipse>& ecl, float t, float darkness,
             bool forbidTint) {
    constexpr int M = lf::kGrid + 2 * kPad;  // клеток по стороне вместе с краем
    constexpr int N = M + 1;
    static float L[N * N];
    for (int jj = 0; jj < N; jj++)
        for (int ii = 0; ii < N; ii++) {
            float i = (float)(ii - kPad), j = (float)(jj - kPad);
            float best = 0;
            for (const auto& s : src) {
                float d = std::hypot(i - s.x, j - s.y);
                best = std::max(best, 1.f - smoothstep(s.r - 1.4f, s.r + 0.7f, d));
            }
            for (const auto& e : ecl) {
                float d = std::hypot(i - e.x, j - e.y);
                best *= smoothstep(e.r - 1.2f, e.r + 0.3f, d);
            }
            L[jj * N + ii] = best;
        }
    std::copy(L, L + N * N, gLight);
    gDark = darkness;
    gLightValid = true;
    const uint32_t fog = rgba(14, 9, 30), red = rgba(150, 20, 30);
    for (int jj = 0; jj < M; jj++)
        for (int ii = 0; ii < M; ii++) {
            float i = (float)(ii - kPad), j = (float)(jj - kPad);
            Vec2 a = cam.toScreen(i, j), b = cam.toScreen(i + 1.f, j), c = cam.toScreen(i + 1.f, j + 1.f), d = cam.toScreen(i, j + 1.f);
            if (b.x < 0 || d.x > cam.w || a.y > cam.h || c.y < 0) continue;
            float la = L[jj * N + ii], lb = L[jj * N + ii + 1], lc = L[(jj + 1) * N + ii + 1], ld = L[(jj + 1) * N + ii];
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
        ellipseSprite("fx/glow", p.x, p.y, rx * 0.35f, rx * 0.175f, rgba(255, 200, 120, (int)(40 + 14 * std::sin(t * 7 + s.y))), true);
    }
    // клубы Мглы медленно ползут по тёмным участкам
    const float W = lf::kGrid + 2.f * kPad;
    for (int i = 0; i < 34; i++) {
        uint32_t h = hash2(i * 31 + 7, i * 17 + 3);
        float bx = (h % 1000) / 1000.f * W, by = ((h >> 10) % 1000) / 1000.f * W - kPad;
        float wx = std::fmod(bx + t * (0.12f + (h >> 24) % 5 * 0.03f), W) - kPad;
        float wy = by + std::sin(t * 0.07f + i * 1.7f) * 2.5f;
        float a = (1 - lightAt(wx, wy)) * darkness * 0.42f * std::min(1.f, std::min(wx + kPad, W - kPad - wx) / 3.f);
        if (a < 0.02f) continue;
        Vec2 p = cam.toScreen(wx, wy, 8);
        float r = (2.2f + (h >> 20) % 4 * 0.6f) * kTW * cam.zoom / 2;
        if (p.x < -r || p.x > cam.w + r || p.y < -r || p.y > cam.h + r) continue;
        ellipseSprite("fx/soft", p.x, p.y, r, r * 0.42f, rgba(60, 40, 110, (int)(255 * a)));
    }
}

void glowSpot(float x, float y, float r, uint32_t color) { glowAt(x, y, r, color); }

void updateParticles(std::vector<Particle>& ps, float dt) {
    for (auto& p : ps) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        if (p.chunk) {
            p.vz -= 520 * dt;
            if (p.z < 0) { p.z = 0; p.vz = -p.vz * 0.3f; p.vx *= 0.5f; p.vy *= 0.5f; }
        }
        p.life -= dt;
    }
    ps.erase(std::remove_if(ps.begin(), ps.end(), [](const Particle& p) { return p.life <= 0; }), ps.end());
}

float lightAt(float x, float y) {
    if (!gLightValid) return 1.f;
    float fx = std::clamp(x + kPad, 0.f, (float)(kLM - 1.001f)), fy = std::clamp(y + kPad, 0.f, (float)(kLM - 1.001f));
    int i = (int)fx, j = (int)fy;
    float u = fx - i, v = fy - j;
    float a = gLight[j * kLM + i], b = gLight[j * kLM + i + 1], c = gLight[(j + 1) * kLM + i], d = gLight[(j + 1) * kLM + i + 1];
    return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v;
}

uint32_t lightTint(float x, float y, uint32_t base) {
    // во тьме — холодный лунный оттенок, у огня — чуть тёплый
    float l = lightAt(x, y), k = gDark * (1 - l) * 0.85f;
    float r = 1 - k * 0.62f, g = 1 - k * 0.64f, bl = 1 - k * 0.38f;
    if (l > 0.5f) { float w = (l - 0.5f) * 0.12f; r = std::min(1.f, r + w); bl -= w; }
    auto ch = [](uint32_t c, int sh, float m) { return (uint32_t)std::clamp((float)((c >> sh) & 255) * m, 0.f, 255.f); };
    return ch(base, 0, r) | ch(base, 8, g) << 8 | ch(base, 16, bl) << 16 | (base & 0xff000000u);
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
    const atlas::Sprite* sp = buildingSprite(type, level);
    if (const atlas::Sprite* ts = turretSprite(type, level, look.angle)) sp = ts;
    if (look.dead) {
        drawFootprint(cam, x + 0.1f, y + 0.1f, s - 0.2f, rgba(30, 24, 30, 150));
        uint32_t h = hash2((int)x * 7, (int)y * 13);
        float appear = look.collapse < 0 ? 1.f : std::clamp(look.collapse * 1.6f, 0.f, 1.f);
        for (int k = 0; k < (int)s * 4; k++) {
            float px = x + 0.3f + ((h >> (k % 16)) & 7) / 7.f * (s - 0.6f);
            float py = y + 0.3f + ((h >> ((k * 3) % 16)) & 7) / 7.f * (s - 0.6f);
            Vec2 p = cam.toScreen(px, py, 3);
            float r = (5 + ((h >> (k % 11)) & 3) * 1.5f) * cam.zoom * appear;
            ellipseSprite("fx/soft", p.x, p.y, r * 1.3f, r * 0.8f, lightTint(px, py, k % 2 ? rgba(70, 64, 72, 235) : rgba(96, 74, 58, 235)));
        }
        // обрушение: здание уходит в землю, сплющиваясь и темнея
        if (sp && look.collapse >= 0 && look.collapse < 1) {
            float k = look.collapse, e = k * k;
            float sc = cam.zoom * 0.5f;
            float w = sp->w * sc, hh = sp->h * sc * (1 - e * 0.75f);
            float jx = std::sin(k * 70) * 3 * (1 - k) * cam.zoom;
            float bx = c.x - sp->px * sc + jx, by = c.y - sp->py * sc * (1 - e * 0.75f) + e * 10 * cam.zoom;
            int v = (int)(255 * (1 - k * 0.6f));
            atlas::drawRect(sp, bx, by, w, hh, lightTint(x + s / 2, y + s / 2, rgba(v, v, v, (int)(255 * (1 - e)))));
        }
        return;
    }
    float dim = look.dim * (look.constructing ? 0.7f : 1.f);
    uint32_t col = rgba((int)(255 * dim), (int)(255 * dim), (int)(255 * std::min(1.f, dim * 1.08f)), (int)(255 * look.alpha * (look.constructing ? 0.75f : 1)));
    if (!look.ghost) col = lightTint(x + s / 2, y + s / 2, col);
    if (look.hitFlash > 0) c.x += std::sin(t * 90) * look.hitFlash * 6 * cam.zoom;
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

namespace {
// Спрайт юнита: anim — move (8 кадров), attack (6), idle (4), die (5).
const atlas::Sprite* unitSprite(const std::string& unit, const char* anim, float angle, int frame) {
    float deg = angle * 57.2958f;
    int dir = ((int)std::lround((deg - 45.f) / 45.f) % 8 + 8) % 8;
    std::string name = "u/" + unit + "/" + anim + "_" + std::to_string(dir) + "_" + std::to_string(frame);
    if (const atlas::Sprite* sp = atlas::get(name)) return sp;
    return atlas::get("u/" + unit + "/move_" + std::to_string(dir) + "_0");
}
}  // namespace

void drawUnit(const Camera& cam, const std::string& unit, const std::string& anim, float angle, int frame, float x, float y, float alpha) {
    const atlas::Sprite* sp = unitSprite(unit, anim.c_str(), angle, frame);
    if (!sp) return;
    Vec2 p = cam.toScreen(x, y);
    atlas::draw(sp, p.x, p.y, cam.zoom * 0.5f, lightTint(x, y, rgba(255, 255, 255, (int)(255 * alpha))));
}

void drawParticle(const Camera& cam, const Particle& p) {
    Vec2 s = cam.toScreen(p.x, p.y, p.z);
    float k = p.life / p.max;
    float r = p.size * cam.zoom * (p.additive ? 1.f : (1.6f - 0.6f * k));
    uint32_t c = gfx::withAlpha(p.color, std::min(1.f, k * 2.f) * ((p.color >> 24) & 255) / 255.f);
    if (p.additive) glowAt(s.x, s.y, r, c);
    else ellipseSprite(p.chunk ? "fx/dot" : "fx/soft", s.x, s.y, r, p.chunk ? r * 0.8f : r, c);
}

void drawTroop(const Camera& cam, const lf::BTroop& tr, float t, float deadT) {
    float lift = tr.air ? 46 + std::sin(t * 4 + tr.bob) * 4 : 0;
    if (deadT >= 0 && tr.air) lift *= std::max(0.f, 1 - deadT * 2.5f);  // летуны падают
    Vec2 g = cam.toScreen(tr.x, tr.y);
    Vec2 p = cam.toScreen(tr.x, tr.y, lift);
    float alpha = tr.hidden ? 0.4f : 1.f;
    if (deadT >= 0) alpha *= std::clamp(1.f - (deadT - 1.6f) / 1.2f, 0.f, 1.f);
    if (alpha <= 0) return;
    if (tr.air && deadT < 0) ellipseSprite("fx/shadow", g.x, g.y, 12 * cam.zoom, 6 * cam.zoom, rgba(0, 0, 0, 90));
    const char* anim;
    int f;
    if (deadT >= 0) {
        anim = "die";
        f = std::min(4, (int)(deadT * 10));
    } else if (tr.attacking) {
        // удар (середина анимации) совпадает с моментом, когда перезарядка сбрасывается
        float ph = tr.def->cd > 0 ? 1 - tr.cd / tr.def->cd : 0;
        anim = "attack";
        f = (int)(std::fmod(ph + 0.5f, 1.f) * 6) % 6;
    } else {
        anim = "move";
        f = (int)(tr.animT * 12 * tr.def->speed / 1.5f + tr.bob * 4) % 8;
    }
    const atlas::Sprite* sp = unitSprite(tr.def->id, anim, tr.angle, f);
    uint32_t col = lightTint(tr.x, tr.y, rgba(255, 255, 255, (int)(255 * alpha)));
    if (tr.def->haze) {
        glowAt(p.x, p.y - 14 * cam.zoom, 22 * cam.zoom, rgba(150, 90, 220, (int)(60 * alpha)));
        col = rgba(190, 170, 230, (int)(235 * alpha));
    }
    if (sp) atlas::draw(sp, p.x, p.y, cam.zoom * 0.5f, col);
    else ellipseSprite("fx/soft", p.x, p.y - 8 * cam.zoom, 8 * cam.zoom, 10 * cam.zoom, tr.def->color);
    if (deadT >= 0) return;
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

void drawSpellZone(const Camera& cam, const lf::SpellZone& z, float battleTime, float t) {
    float left = z.until - battleTime;
    float fade = std::clamp(left / 0.6f, 0.f, 1.f) * std::clamp((lf::kSpellDuration - left) / 0.3f, 0.f, 1.f);
    bool heal = z.kind == lf::Spell::Heal;
    uint32_t rim = heal ? rgba(140, 255, 150) : rgba(255, 90, 80), fill = heal ? rgba(90, 230, 110) : rgba(230, 50, 60);
    Vec2 p = cam.toScreen(z.x, z.y);
    float rx = z.r * kTW * cam.zoom / kSqrt2;
    float pulse = 1 + std::sin(t * 6) * 0.03f;
    ellipseSprite("fx/glow", p.x, p.y, rx * pulse, rx / 2 * pulse, gfx::withAlpha(fill, 0.35f * fade), true);
    ellipseSprite("fx/ring", p.x, p.y, rx, rx / 2, gfx::withAlpha(rim, 0.9f * fade));
    ellipseSprite("fx/ring", p.x, p.y, rx * 0.7f, rx * 0.35f, gfx::withAlpha(rim, 0.35f * fade));
    // поднимающиеся искры
    for (int i = 0; i < 14; i++) {
        uint32_t h = hash2(i * 13 + (int)(z.x * 10), i * 7 + (int)(z.y * 10));
        float a = (h % 628) / 100.f, r = ((h >> 10) % 100) / 100.f * z.r * 0.9f;
        float ph = std::fmod(t * 0.7f + (h >> 20) % 100 / 100.f, 1.f);
        Vec2 q = cam.toScreen(z.x + std::cos(a) * r, z.y + std::sin(a) * r, ph * 60);
        glowAt(q.x, q.y, (heal ? 6 : 7) * cam.zoom + 2, gfx::withAlpha(rim, (1 - ph) * fade));
    }
}

void drawBuildingThumb(lf::BType type, float cx, float cy, float maxW, float maxH, int level) {
    const atlas::Sprite* sp = buildingSprite(type, level);
    if (!sp) return;
    float k = std::min(maxW / sp->w, maxH / sp->h);
    atlas::drawRect(sp, cx - sp->w * k / 2, cy - sp->h * k / 2, sp->w * k, sp->h * k);
}

void drawTroopThumb(lf::TroopType type, float cx, float cy, float size) {
    const atlas::Sprite* sp = unitSprite(lf::T(type).id, "idle", 0.785f, 0);
    if (!sp) return;
    float k = size / std::max(sp->w, sp->h);
    atlas::drawRect(sp, cx - sp->w * k / 2, cy - sp->h * k / 2, sp->w * k, sp->h * k);
}

}  // namespace client::world
