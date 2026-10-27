#include "world.h"

#include <algorithm>
#include <cmath>

#include "engine/atlas.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "platform/platform.h"

namespace client::world {
using gfx::rgba;

namespace {
constexpr float kSqrt2 = 1.41421356f;

// Общий оттенок освещения сцены (день — белый, сумерки набега — тёплый и тусклый).
uint32_t gAmbient = 0xffffffffu;

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
    // свой облик на каждом уровне (_l0 — руины), если он есть
    if (const atlas::Sprite* s = atlas::get(std::string("b/") + def.id + "_l" + std::to_string(level))) return s;
    level = std::max(1, level);
    int tier = std::clamp((level - 1) * 3 / std::max(1, def.maxLevel) + 1, 1, 3);
    for (int t = tier; t >= 1; t--)
        if (const atlas::Sprite* s = atlas::get(std::string("b/") + def.id + "_t" + std::to_string(t))) return s;
    return atlas::get(std::string("b/") + def.id);
}

namespace {
// Поворотная часть (Самострел, Пушка): спрайты _d0.._d7 — поворот на 45° против часовой (вид сверху в Blender).
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

namespace {
const gfx::Texture* groundTex() {
    static const gfx::Texture* t = gfx::loadTexture(platform::assetDir() + "/art/ground.jpg", true);
    return t;
}
const gfx::Texture* grassTex() {
    static const gfx::Texture* t = gfx::loadTexture(platform::assetDir() + "/art/grass_tile.png", true);
    return t;
}
}  // namespace

// Ручей и пруд — те же формулы, что в art/make_terrain.py.
float streamCenter(float t) { return 47.6f + 1.1f * std::sin(t * 0.21f) + 0.45f * std::sin(t * 0.53f + 1.f); }
constexpr float kPondX = 48.5f, kPondY = 48.5f, kPondR = 3.6f;

float waterDist(float x, float y) {
    float da = x <= 48.5f ? std::fabs(y - streamCenter(x)) : 99.f;
    float db = y <= 48.5f ? std::fabs(x - streamCenter(y)) : 99.f;
    float ds = std::min(da, db) - (0.75f + 0.3f * std::sin(std::min(x, y) * 0.37f));
    float dp = std::hypot(x - kPondX, y - kPondY) - kPondR;
    return std::min(ds, dp);
}

void drawGround(const Camera& cam) {
    const float a = -kPad, b = lf::kGrid + kPad;
    if (const gfx::Texture* g = groundTex()) {
        // вся земля — один квад: изометрия аффинна, текстура «вид сверху» ложится без швов
        Vec2 p0 = cam.toScreen(a, a), p1 = cam.toScreen(b, a), p2 = cam.toScreen(b, b), p3 = cam.toScreen(a, b);
        const uint32_t w = 0xffffffffu;
        gfx::Vtx v[4] = {{p0.x, p0.y, 0, 0, w}, {p1.x, p1.y, 1, 0, w}, {p2.x, p2.y, 1, 1, w}, {p3.x, p3.y, 0, 1, w}};
        gfx::quad(g, v);
    } else {
        const float s = cam.zoom * 0.5f;
        for (int j = -kPad; j < lf::kGrid + kPad; j++)
            for (int i = -kPad; i < lf::kGrid + kPad; i++) {
                Vec2 p = cam.toScreen(i + 0.5f, j + 0.5f);
                uint32_t h = hash2(i + 100, j + 100);
                atlas::draw(atlas::get("g/grass" + std::to_string(h % 6)), p.x, p.y, s);
            }
    }
    // настоящая трава (отрендерена в Blender, плитка 4×4 клетки) с переливами оттенков по полю
    const gfx::Texture* gt = grassTex();
    if (!gt) return;
    auto vcol = [](float x, float y) {
        float n = std::sin(x * 0.31f + 1.3f) * std::sin(y * 0.27f + 0.4f) * 0.5f + std::sin((x + y) * 0.13f) * 0.5f;  // -1..1
        float out = std::max({-x, -y, x - lf::kGrid, y - lf::kGrid, 0.f});
        float a = std::clamp(1.f - out / 2.2f, 0.f, 1.f);
        float k = 0.93f + 0.07f * n;
        return rgba((int)(255 * std::min(1.f, k * (1.f + 0.05f * n))), (int)(255 * k), (int)(255 * k * (0.95f - 0.05f * n)), (int)(255 * a));
    };
    for (int j = -2; j < lf::kGrid + 2; j += 2)
        for (int i = -2; i < lf::kGrid + 2; i += 2) {
            float x0 = (float)i, y0 = (float)j, x1 = x0 + 2, y1 = y0 + 2;
            Vec2 q0 = cam.toScreen(x0, y0), q1 = cam.toScreen(x1, y0), q2 = cam.toScreen(x1, y1), q3 = cam.toScreen(x0, y1);
            if (q1.x < 0 || q3.x > cam.w || q0.y > cam.h || q2.y < 0) continue;
            float u0 = (((i % 4) + 4) % 4) / 4.f, v0 = (((j % 4) + 4) % 4) / 4.f;
            gfx::Vtx v[4] = {{q0.x, q0.y, u0, v0, vcol(x0, y0)}, {q1.x, q1.y, u0 + 0.5f, v0, vcol(x1, y0)},
                             {q2.x, q2.y, u0 + 0.5f, v0 + 0.5f, vcol(x1, y1)}, {q3.x, q3.y, u0, v0 + 0.5f, vcol(x0, y1)}};
            gfx::quad(gt, v);
        }
}

// Мягкая тень у основания постройки: здание «стоит» на траве без подложки.
void drawContactShadow(const Camera& cam, float x, float y, float size) {
    Vec2 c = cam.toScreen(x + size / 2, y + size / 2);
    float rx = size * kTW * cam.zoom / 2, ry = rx / 2;
    ellipseSprite("fx/soft", c.x + rx * 0.06f, c.y + ry * 0.08f, rx * 0.92f, ry * 0.92f, rgba(8, 20, 6, 70));
    ellipseSprite("fx/soft", c.x + rx * 0.04f, c.y + ry * 0.05f, rx * 0.66f, ry * 0.66f, rgba(8, 18, 6, 60));
}

namespace {
struct Detail { const atlas::Sprite* sp; float x, y; };
std::vector<Detail>& meadow() {
    static std::vector<Detail> v;
    static bool built = false;
    if (built) return v;
    built = true;
    // рисованные мелочи луга (art/source/generated/sheet_ground): трава, высокая трава, папоротник, колокольчики, маки,
    // ромашки, камешки, грибы — трава чаще прочего
    // gd1 трава, gd2 высокая трава, gd16 папоротник, gd3/gd5 колокольчики, gd8 маки, gd13 ромашки, gd14 грибы
    static const char* kinds[] = {"b/decor_gd1", "b/decor_gd2", "b/decor_gd1", "b/decor_gd16", "b/decor_gd3", "b/decor_gd8",
                                  "b/decor_gd13", "b/decor_gd1", "b/decor_gd2", "b/decor_gd14", "b/decor_gd5"};
    for (int j = lf::kBorder; j < lf::kGrid - lf::kBorder; j++)
        for (int i = lf::kBorder; i < lf::kGrid - lf::kBorder; i++) {
            uint32_t h = hash2(i * 3 + 7, j * 5 + 11);
            if (h % 100 >= 15) continue;  // реже: луг не должен спорить с постройками
            const atlas::Sprite* sp = atlas::get(kinds[(h >> 8) % 11]);
            if (!sp) continue;
            v.push_back({sp, i + 0.2f + ((h >> 12) & 15) / 25.f, j + 0.2f + ((h >> 16) & 15) / 25.f});
        }
    return v;
}
}  // namespace

namespace {
// Спрайт со сдвигом верхнего края (ветер качает траву).
void drawSway(const atlas::Sprite* sp, float x, float y, float sc, float shear, uint32_t c) {
    float l = x - sp->px * sc, t = y - sp->py * sc, r = l + sp->w * sc, b = t + sp->h * sc, sh = shear * sp->h * sc;
    gfx::Vtx v[4] = {{l + sh, t, sp->u0, sp->v0, c}, {r + sh, t, sp->u1, sp->v0, c}, {r, b, sp->u1, sp->v1, c}, {l, b, sp->u0, sp->v1, c}};
    gfx::quad(sp->tex, v);
}
}  // namespace

void drawMeadow(const Camera& cam, float t) {
    // тени облаков медленно ползут по полю
    for (int i = 0; i < 5; i++) {
        float W = lf::kGrid + 2.f * kPad;
        float cx = std::fmod(i * 17.3f + t * 0.35f, W) - kPad, cy = std::fmod(i * 23.7f + t * 0.22f, W) - kPad;
        Vec2 p = cam.toScreen(cx, cy);
        float r = (5 + i % 3 * 1.5f) * kTW * cam.zoom / kSqrt2;
        ellipseSprite("fx/soft", p.x, p.y, r, r * 0.5f, rgba(10, 20, 30, 34));
    }
    // ветер: порывы бегут волной по диагонали
    for (const auto& d : meadow()) {
        Vec2 p = cam.toScreen(d.x, d.y);
        if (p.x < -40 || p.x > cam.w + 40 || p.y < -40 || p.y > cam.h + 40) continue;
        float gust = std::sin(t * 1.7f - (d.x + d.y) * 0.35f) * 0.5f + 0.5f;
        float shear = (gust * gust * 0.22f + std::sin(t * 5 + d.x * 3) * 0.03f);
        drawSway(d.sp, p.x, p.y, cam.zoom * 0.4f, shear, 0xffffffffu);
    }
    // бабочки над лугом
    static const uint32_t wings[] = {rgba(255, 220, 90), rgba(250, 250, 240), rgba(255, 150, 70), rgba(170, 200, 255)};
    for (int i = 0; i < 7; i++) {
        float ph = i * 1.9f;
        float bx = 22 + std::cos(t * 0.23f + ph) * (4 + i % 3) + std::sin(t * 0.7f + ph * 2) * 0.8f;
        float by = 22 + std::sin(t * 0.19f + ph * 1.3f) * (4 + (i + 1) % 3) + std::cos(t * 0.9f + ph) * 0.8f;
        Vec2 p = cam.toScreen(bx, by, 22 + std::sin(t * 2.1f + ph) * 8);
        float flap = std::fabs(std::sin(t * 16 + ph)) * 4.5f * cam.zoom + 0.8f;
        ellipseSprite("fx/dot", p.x - flap * 0.6f, p.y, flap, 2.6f * cam.zoom, wings[i % 4]);
        ellipseSprite("fx/dot", p.x + flap * 0.6f, p.y, flap, 2.6f * cam.zoom, wings[i % 4]);
    }
}

void drawWaterFx(const Camera& cam, float t) {
    // блики бегут по течению ручья к пруду
    gfx::setBlend(gfx::Blend::Add);
    for (int i = 0; i < 70; i++) {
        uint32_t h = hash2(i * 7 + 1, i * 13 + 5);
        float len = 57.5f;
        float s = std::fmod((h % 1000) / 1000.f * len + t * 0.9f, len) - kPad;  // от края мира к углу
        float off = ((h >> 10) % 100 / 100.f - 0.5f) * 1.1f;
        float x, y;
        if (i % 2) { x = s; y = streamCenter(s) + off; } else { y = s; x = streamCenter(s) + off; }
        if (s > 47) continue;
        float tw = 0.5f + 0.5f * std::sin(t * 4 + i);
        Vec2 p = cam.toScreen(x, y);
        float r = (3 + (h >> 20) % 3) * cam.zoom;
        atlas::drawRect(atlas::get("fx/soft"), p.x - r * 1.8f, p.y - r * 0.5f, r * 3.6f, r, rgba(150, 210, 255, (int)(70 * tw)));
    }
    for (int i = 0; i < 16; i++) {  // рябь на пруду
        uint32_t h = hash2(i * 31, i * 3 + 9);
        float a = (h % 628) / 100.f, rr = ((h >> 10) % 100) / 100.f * (kPondR - 0.6f);
        float k = std::fmod(t * 0.4f + (h >> 20) % 100 / 100.f, 1.f);
        Vec2 p = cam.toScreen(kPondX + std::cos(a) * rr, kPondY + std::sin(a) * rr);
        float rx = (4 + k * 18) * cam.zoom;
        atlas::drawRect(atlas::get("fx/ring"), p.x - rx, p.y - rx / 2, rx * 2, rx, rgba(150, 210, 255, (int)(70 * (1 - k))));
    }
    gfx::setBlend(gfx::Blend::Alpha);
}

const std::vector<Decor>& decor() {
    static std::vector<Decor> v;
    static bool built = false;
    if (built) return v;
    built = true;
    constexpr int M = lf::kGrid + 2 * kPad;
    std::vector<uint8_t> used(M * M, 0);
    auto idx = [&](int i, int j) { return (j + kPad) * M + (i + kPad); };
    auto mark = [&](float x, float y, int sz) {
        for (int dy = 0; dy < sz; dy++)
            for (int dx = 0; dx < sz; dx++) {
                int i = (int)std::floor(x) + dx, j = (int)std::floor(y) + dy;
                if (i >= -kPad && j >= -kPad && i < lf::kGrid + kPad && j < lf::kGrid + kPad) used[idx(i, j)] = 1;
            }
    };
    auto put = [&](const char* sp, float cx, float cy, int sz, int fx = 0, float dz = 0) {
        float x = cx - sz / 2.f, y = cy - sz / 2.f;
        v.push_back({sp, x, y, sz, x + y + sz + dz, fx});
        mark(x, y, sz);
    };
    // мосты там, где тропы пересекают ручей (рисованный лист art/source/generated/sheet_special)
    put("b/decor_sp1", 22.4f, streamCenter(22.4f), 2, 0, -0.6f);
    put("b/decor_sp1m", streamCenter(22.4f), 22.4f, 2, 0, -0.6f);
    // кувшинки
    const float lil[][2] = {{47.4f, 49.4f}, {49.8f, 47.6f}, {49.3f, 50.1f}, {47.0f, 47.9f}, {50.6f, 49.0f}, {12.5f, streamCenter(12.5f)}, {streamCenter(31.f), 31.f}};
    for (int i = 0; i < 7; i++) put(i % 2 ? "b/decor_lily_small" : "b/decor_lily", lil[i][0], lil[i][1], 1, 2, -1.f);
    // тыквенные грядки слева
    for (int yy = 26; yy < 35; yy += 2)
        for (int xx = -7; xx <= -3; xx += 2) put("b/decor_sp7", xx + 0.5f, yy + 0.5f + (xx & 2) * 0.25f, 1);
    // стоянка путников: палатка, костёр с указателем, скамья из бревна
    put("b/decor_sp3", -5.5f, 11.f, 2);
    put("b/decor_sp2", -3.f, 13.f, 1, 1);
    put("b/decor_sp9", -2.f, 14.6f, 1);
    // руины старого маяка
    put("b/decor_sp6", 6.f, -5.5f, 2);
    put("b/decor_sp4", 9.f, -3.5f, 1);
    put("b/decor_sp5", 3.f, -3.2f, 1);
    put("b/decor_sp8", 11.f, -6.5f, 1);
    put("b/decor_prop3", 1.5f, -6.f, 1);
    // лес и скалы по остальному краю
    // рисованный декор (art/source/generated/sheet_trees, sheet_props): дуб, ель, берёза, дуб, рябина, яблоня;
    // скала, валуны, кусты, цветущий пень, пень, грибы
    static const char* trees[] = {"b/decor_tree1", "b/decor_tree2", "b/decor_tree3", "b/decor_tree5", "b/decor_tree2", "b/decor_tree6",
                                  "b/decor_tree7", "b/decor_tree2"};
    static const char* small[] = {"b/decor_prop2", "b/decor_prop3", "b/decor_prop4", "b/decor_prop5", "b/decor_prop6", "b/decor_prop7",
                                  "b/decor_prop8"};
    auto blocked = [&](float x, float y) {
        if (waterDist(x, y) < 1.3f) return true;                                         // ручей с берегами
        if (y > lf::kGrid - 1 && std::fabs(x - 22.4f) < 1.6f) return true;               // тропы
        if (x > lf::kGrid - 1 && std::fabs(y - 22.4f) < 1.6f) return true;
        return false;
    };
    for (int j = -kPad; j < lf::kGrid + kPad - 1; j++)
        for (int i = -kPad; i < lf::kGrid + kPad - 1; i++) {
            bool outside = i < 0 || j < 0 || i >= lf::kGrid || j >= lf::kGrid;
            if (!outside) continue;
            uint32_t h = hash2(i * 13 + 5, j * 17 + 3);
            int dist = std::max({-i, -j, i - lf::kGrid + 1, j - lf::kGrid + 1});  // насколько далеко за краем
            int chance = dist <= 1 ? 18 : 42;
            if ((int)(h % 100) >= chance) continue;
            bool big = (h >> 9) % 100 < 60 && i + 1 < lf::kGrid + kPad && j + 1 < lf::kGrid + kPad;
            int sz = big ? 2 : 1;
            bool ok = true;
            for (int dy = 0; dy < sz && ok; dy++)
                for (int dx = 0; dx < sz && ok; dx++)
                    if (used[idx(i + dx, j + dy)] || blocked(i + dx + 0.5f, j + dy + 0.5f)) ok = false;
            if (!ok) continue;
            for (int dy = 0; dy < sz; dy++)
                for (int dx = 0; dx < sz; dx++) used[idx(i + dx, j + dy)] = 1;
            const char* name = big ? ((h >> 13) % 6 == 0 ? "b/decor_prop1" : trees[(h >> 15) % 8]) : small[(h >> 15) % 7];
            v.push_back({name, (float)i, (float)j, sz, i + j + sz * 1.f});
        }
    return v;
}

void drawDecor(const Camera& cam, const Decor& d, float t) {
    float cx = d.x + d.size / 2.f, cy = d.y + d.size / 2.f;
    Vec2 c = cam.toScreen(cx, cy);
    if (c.x < -200 || c.x > cam.w + 200 || c.y < -60 || c.y > cam.h + 300) return;
    const atlas::Sprite* sp = atlas::get(d.sprite);
    if (d.fx == 2) c.y += std::sin(t * 1.3f + cx) * 1.2f * cam.zoom;  // кувшинка покачивается
    uint32_t col = ambientTint(rgba(235, 235, 235));
    if (d.fx == 1) col = rgba(255, 235, 210);
    atlas::draw(sp, c.x, c.y, cam.zoom * 0.5f, col);
    if (d.fx == 1) {  // костёр: тёплый отсвет
        float fl = 0.85f + 0.15f * std::sin(t * 9) * std::sin(t * 5.3f);
        float r = 3.2f * kTW * cam.zoom / kSqrt2 * fl;
        ellipseSprite("fx/glow", c.x, c.y, r, r / 2, rgba(255, 150, 60, 70), true);
        glowAt(c.x, c.y - 10 * cam.zoom, 16 * cam.zoom * fl, rgba(255, 190, 90, 220));
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

void setAmbient(uint32_t color) { gAmbient = color; }

uint32_t ambientTint(uint32_t base) {
    auto ch = [](uint32_t c, uint32_t m, int sh) { return (((c >> sh) & 255) * ((m >> sh) & 255) / 255) << sh; };
    return ch(base, gAmbient, 0) | ch(base, gAmbient, 8) | ch(base, gAmbient, 16) | (base & 0xff000000u);
}

void drawGridAround(const Camera& cam, float cx, float cy, float r) {
    int x0 = std::max(0, (int)(cx - r)), x1 = std::min(lf::kGrid, (int)(cx + r) + 1);
    int y0 = std::max(0, (int)(cy - r)), y1 = std::min(lf::kGrid, (int)(cy + r) + 1);
    auto alpha = [&](float x, float y) { return std::clamp(1.f - std::hypot(x - cx, y - cy) / r, 0.f, 1.f); };
    for (int i = x0; i <= x1; i++)
        for (int j = y0; j < y1; j++) {
            float a = alpha((float)i, j + 0.5f);
            if (a > 0) line(cam.toScreen((float)i, (float)j), cam.toScreen((float)i, j + 1.f), 1.2f, rgba(255, 255, 255, (int)(70 * a)));
        }
    for (int j = y0; j <= y1; j++)
        for (int i = x0; i < x1; i++) {
            float a = alpha(i + 0.5f, (float)j);
            if (a > 0) line(cam.toScreen((float)i, (float)j), cam.toScreen(i + 1.f, (float)j), 1.2f, rgba(255, 255, 255, (int)(70 * a)));
        }
}

void drawFootprint(const Camera& cam, float x, float y, float size, uint32_t color) {
    Vec2 a = cam.toScreen(x, y), b = cam.toScreen(x + size, y), c = cam.toScreen(x + size, y + size), d = cam.toScreen(x, y + size);
    gfx::Vtx v[4] = {{a.x, a.y, 0, 0, color}, {b.x, b.y, 0, 0, color}, {c.x, c.y, 0, 0, color}, {d.x, d.y, 0, 0, color}};
    gfx::quad(gfx::white(), v);
}

// Контур занятых клеток (выбор и перенос постройки): ромб из четырёх линий.
void drawFootprintOutline(const Camera& cam, float x, float y, float size, uint32_t color, float width) {
    Vec2 p[4] = {cam.toScreen(x, y), cam.toScreen(x + size, y), cam.toScreen(x + size, y + size), cam.toScreen(x, y + size)};
    for (int i = 0; i < 4; i++) {
        Vec2 a = p[i], b = p[(i + 1) % 4];
        float dx = b.x - a.x, dy = b.y - a.y, len = std::max(1e-3f, std::hypot(dx, dy));
        float nx = -dy / len * width / 2, ny = dx / len * width / 2;
        gfx::Vtx v[4] = {{a.x + nx, a.y + ny, 0, 0, color}, {b.x + nx, b.y + ny, 0, 0, color}, {b.x - nx, b.y - ny, 0, 0, color}, {a.x - nx, a.y - ny, 0, 0, color}};
        gfx::quad(gfx::white(), v);
    }
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
    if (look.ghost) {
        uint32_t fill = look.ghost > 0 ? rgba(90, 255, 120, 70) : rgba(255, 70, 60, 90), line = look.ghost > 0 ? rgba(150, 255, 160, 230) : rgba(255, 120, 100, 240);
        drawFootprint(cam, x, y, s, fill);
        drawFootprintOutline(cam, x, y, s, line, 2.5f);
    }
    if (look.selected) {  // квадрат подсветки занятых клеток вместо подложки
        float pulse = 0.5f + 0.5f * std::sin(t * 5);
        drawFootprint(cam, x, y, s, rgba(255, 236, 160, (int)(34 + 26 * pulse)));
        drawFootprintOutline(cam, x, y, s, rgba(255, 226, 130, (int)(170 + 70 * pulse)), 2.5f);
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
            ellipseSprite("fx/soft", p.x, p.y, r * 1.3f, r * 0.8f, ambientTint(k % 2 ? rgba(70, 64, 72, 235) : rgba(96, 74, 58, 235)));
        }
        // обрушение: здание уходит в землю, сплющиваясь и темнея
        if (sp && look.collapse >= 0 && look.collapse < 1) {
            float k = look.collapse, e = k * k;
            float sc = cam.zoom * 0.5f;
            float w = sp->w * sc, hh = sp->h * sc * (1 - e * 0.75f);
            float jx = std::sin(k * 70) * 3 * (1 - k) * cam.zoom;
            float bx = c.x - sp->px * sc + jx, by = c.y - sp->py * sc * (1 - e * 0.75f) + e * 10 * cam.zoom;
            int v = (int)(255 * (1 - k * 0.6f));
            atlas::drawRect(sp, bx, by, w, hh, ambientTint(rgba(v, v, v, (int)(255 * (1 - e)))));
        }
        return;
    }
    float dim = look.dim * (look.constructing ? 0.7f : 1.f);
    uint32_t col = rgba((int)(255 * dim), (int)(255 * dim), (int)(255 * std::min(1.f, dim * 1.08f)), (int)(255 * look.alpha * (look.constructing ? 0.75f : 1)));
    if (!look.ghost) col = ambientTint(col);
    if (look.hitFlash > 0) c.x += std::sin(t * 90) * look.hitFlash * 6 * cam.zoom;
    bool popping = look.pop >= 0 && look.pop < 1;
    if (popping) gfx::pushScale(c.x, c.y, 1 + 0.13f * std::sin(look.pop * 9.42f) * (1 - look.pop));  // затухающая пружина от основания
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
    if (popping) gfx::popScale();
}

namespace {
// Спрайт юнита: anim — move (8 кадров), attack (6), idle (4), die (5). Направления 5..7 — зеркала 3..1 (flip).
const atlas::Sprite* unitSprite(const std::string& unit, const char* anim, float angle, int frame, bool* flip = nullptr) {
    float deg = angle * 57.2958f;
    int dir = ((int)std::lround((deg - 45.f) / 45.f) % 8 + 8) % 8;
    bool mirrored = dir > 4;
    if (mirrored) dir = 8 - dir;
    if (flip) *flip = mirrored;
    std::string name = "u/" + unit + "/" + anim + "_" + std::to_string(dir) + "_" + std::to_string(frame);
    if (const atlas::Sprite* sp = atlas::get(name)) return sp;
    return atlas::get("u/" + unit + "/move_" + std::to_string(dir) + "_0");
}
}  // namespace

void drawUnit(const Camera& cam, const std::string& unit, const std::string& anim, float angle, int frame, float x, float y, float alpha) {
    bool flip = false;
    const atlas::Sprite* sp = unitSprite(unit, anim.c_str(), angle, frame, &flip);
    if (!sp) return;
    Vec2 p = cam.toScreen(x, y);
    atlas::draw(sp, p.x, p.y, cam.zoom * 0.5f, ambientTint(rgba(255, 255, 255, (int)(255 * alpha))), flip);
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
    float alpha = 1.f;
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
    bool flip = false;
    const atlas::Sprite* sp = unitSprite(tr.def->id, anim, tr.angle, f, &flip);
    uint32_t col = ambientTint(rgba(255, 255, 255, (int)(255 * alpha)));
    if (sp) atlas::draw(sp, p.x, p.y, cam.zoom * 0.5f, col, flip);
    else ellipseSprite("fx/soft", p.x, p.y - 8 * cam.zoom, 8 * cam.zoom, 10 * cam.zoom, tr.def->color);
    if (deadT >= 0) return;
    if (tr.hitT > 0 && sp) {
        gfx::setBlend(gfx::Blend::Add);
        atlas::draw(sp, p.x, p.y, cam.zoom * 0.5f, rgba(255, 120, 120, 160), flip);
        gfx::setBlend(gfx::Blend::Alpha);
    }
    if (tr.hp < tr.maxHp || tr.type == lf::TroopType::Hero) {
        float w = 22 * std::max(0.7f, cam.zoom);
        float y = p.y - 44 * cam.zoom;
        gfx::fillRect(p.x - w / 2 - 1, y - 1, w + 2, 6, rgba(10, 6, 18, 200));
        gfx::fillRect(p.x - w / 2, y, w * std::max(0.f, tr.hp / tr.maxHp), 4,
                      tr.def->raider ? rgba(230, 90, 70) : tr.type == lf::TroopType::Hero ? rgba(255, 210, 80) : rgba(130, 220, 80));
    }
}

void drawProjectile(const Camera& cam, const lf::Projectile& p) {
    float h0 = p.fromAir ? 46.f : (p.src ? 40.f : 14.f);
    float z = h0 * (1 - p.k) + 12 * p.k;
    if (p.kind == lf::Proj::Ball) z += std::sin(p.k * 3.14159f) * 110;
    Vec2 s = cam.toScreen(p.x, p.y, z);
    switch (p.kind) {
        case lf::Proj::Ball: {  // чугунное ядро
            Vec2 g = cam.toScreen(p.x, p.y);
            ellipseSprite("fx/shadow", g.x, g.y, 6 * cam.zoom, 3 * cam.zoom, rgba(0, 0, 0, 90));
            ellipseSprite("fx/dot", s.x, s.y, 5 * cam.zoom, 5 * cam.zoom, rgba(52, 50, 50));
            break;
        }
        case lf::Proj::Bullet: {  // пуля: короткий светлый след и дымок у ствола
            Vec2 a = cam.toScreen(p.sx, p.sy), b = cam.toScreen(p.tx, p.ty);
            float dx = b.x - a.x, dy = b.y - a.y, l = std::hypot(dx, dy) + 0.001f;
            line({s.x - dx / l * 12 * cam.zoom, s.y - dy / l * 12 * cam.zoom}, s, std::max(1.f, 1.6f * cam.zoom), rgba(255, 236, 190, 200));
            if (p.k < 0.4f) {
                Vec2 m = cam.toScreen(p.sx, p.sy, h0);
                ellipseSprite("fx/soft", m.x, m.y - p.k * 20, (6 + p.k * 30) * cam.zoom, (6 + p.k * 30) * cam.zoom, rgba(220, 220, 215, (int)(150 * (1 - p.k / 0.4f))));
            }
            break;
        }
        case lf::Proj::Bolt:
        case lf::Proj::Arrow: {
            Vec2 a = cam.toScreen(p.sx, p.sy), b = cam.toScreen(p.tx, p.ty);
            float dx = b.x - a.x, dy = b.y - a.y, l = std::hypot(dx, dy) + 0.001f;
            float L = (p.kind == lf::Proj::Bolt ? 16 : 9) * cam.zoom;
            line({s.x - dx / l * L, s.y - dy / l * L}, s, std::max(1.2f, 2.f * cam.zoom), p.kind == lf::Proj::Bolt ? rgba(200, 180, 150) : rgba(190, 170, 130));
            break;
        }
        default:
            ellipseSprite("fx/dot", s.x, s.y, 3 * cam.zoom, 3 * cam.zoom, rgba(60, 50, 40));
    }
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
        case lf::Fx::Pitch: {  // струя кипящей смолы от котла и тёмная лужа с паром
            Vec2 a = cam.toScreen(e.sx, e.sy, 40);
            for (int i = 0; i <= 6; i++) {
                float f = i / 6.f;
                ellipseSprite("fx/soft", a.x + (p.x - a.x) * f, a.y + (p.y - 6 * z - a.y) * f, (3 + f * 5) * z + 1, (3 + f * 5) * z + 1,
                              rgba(40, 28, 18, (int)(220 * (1 - k))));
            }
            float rx = 1.4f * kTW * z / kSqrt2 * (0.5f + k * 0.5f);
            ellipseSprite("fx/soft", p.x, p.y, rx, rx / 2, rgba(30, 22, 14, (int)(170 * (1 - k))));
            ellipseSprite("fx/soft", p.x, p.y - 14 * z - k * 20 * z, rx * 0.6f, rx * 0.5f, rgba(230, 230, 225, (int)(90 * (1 - k))));
            break;
        }
        case lf::Fx::Smoke:
            ellipseSprite("fx/soft", p.x, p.y - 10 * z - k * 30 * z, (8 + k * 18) * z, (8 + k * 18) * z, rgba(200, 200, 195, (int)(140 * (1 - k))));
            break;
        case lf::Fx::Cry: {  // боевой клич: волна, расходящаяся от Воеводы
            float rx = e.r * kTW * z / kSqrt2 * (0.15f + k * 0.85f);
            ellipseSprite("fx/ring", p.x, p.y, rx, rx / 2, rgba(255, 210, 120, (int)(230 * (1 - k))));
            ellipseSprite("fx/ring", p.x, p.y, rx * 0.7f, rx * 0.35f, rgba(255, 180, 90, (int)(160 * (1 - k))));
            break;
        }
        case lf::Fx::Arrival:  // ватага выходит из леса: пыль
            for (int i = 0; i < 5; i++) {
                float a = i * 1.26f;
                float rr = 30 * z * (0.5f + k);
                ellipseSprite("fx/soft", p.x + std::cos(a) * rr, p.y + std::sin(a) * rr * 0.5f - k * 10 * z, 14 * z, 10 * z,
                              rgba(170, 155, 130, (int)(140 * (1 - k))));
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
