#include "surv_game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "engine/atlas.h"
#include "engine/audio.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "engine/ui.h"
#include "sokol_app.h"

using gfx::rgba;
using r3d::Mat4;
using r3d::Vec3;

namespace sclient {
namespace {

constexpr float kVirtualH = 560.f;
constexpr float kPi = 3.14159265f;

uint32_t hashU(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
float h01(uint32_t x) { return (hashU(x) & 0xffff) / 65535.f; }
Vec3 W3(float x, float y, float h = 0) { return {x, h, y}; }
uint32_t shade(uint32_t c, float k) {
    int r = (int)((c & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)(((c >> 16) & 255) * k);
    return rgba(std::min(r, 255), std::min(g, 255), std::min(b, 255), (int)(c >> 24));
}
Vec3 mixv(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
float smooth01(float x) { x = std::clamp(x, 0.f, 1.f); return x * x * (3 - 2 * x); }
// поворот модели: её «вперёд» — локальная +X, в мире — направление facing (x, y земли)
Mat4 placeAt(Vec3 p, float facing, float s = 1.f) { return Mat4::translate(p) * Mat4::rotY(-facing) * Mat4::scale({s, s, s}); }

// ---------- деревья (общие для леса на поляне и стены леса за краем) ----------
void addFir(r3d::Builder& b, const Mat4& xf, uint32_t seed) {
    uint32_t trunk = rgba(104, 70, 46);
    b.cylinder({0, 0, 0}, 0.2f, 0.14f, 1.0f, trunk, 8, xf);
    uint32_t greens[3] = {rgba(46, 104, 62), rgba(40, 92, 56), rgba(54, 116, 68)};
    float y = 0.6f;
    for (int i = 0; i < 4; i++) {
        float r = 1.45f - i * 0.3f;
        uint32_t c = greens[(seed + i) % 3];
        b.cylinder({0, y, 0}, r, 0.0f, 1.55f - i * 0.12f, c, 9, xf, true);
        y += 0.95f - i * 0.08f;
    }
}

void addPine(r3d::Builder& b, const Mat4& xf, uint32_t seed) {
    b.cylinder({0, 0, 0}, 0.2f, 0.12f, 4.4f, rgba(170, 100, 58), 8, xf);
    uint32_t greens[3] = {rgba(78, 132, 60), rgba(68, 120, 54), rgba(88, 142, 66)};
    for (int i = 0; i < 4; i++) {
        float a = i * 2.3f + seed * 0.7f, r = i == 3 ? 0.f : 0.55f;
        Vec3 c(std::cos(a) * r, 3.6f + i * 0.35f, std::sin(a) * r);
        b.sphere(c, {0.95f - i * 0.12f, 0.55f, 0.95f - i * 0.12f}, greens[(seed + i) % 3], 10, 6, xf);
    }
}

void addBirch(r3d::Builder& b, const Mat4& xf, uint32_t seed) {
    b.cylinder({0, 0, 0}, 0.15f, 0.1f, 3.6f, rgba(236, 232, 220), 8, xf);
    for (int i = 0; i < 4; i++) b.cylinder({0, 0.5f + i * 0.75f + (seed % 3) * 0.1f, 0}, 0.155f, 0.15f, 0.07f, rgba(52, 48, 44), 8, xf);
    uint32_t c = (seed & 1) ? rgba(120, 178, 72) : rgba(132, 186, 78);
    b.sphere({0.15f, 3.5f, 0}, {1.0f, 0.85f, 0.95f}, c, 10, 6, xf);
    b.sphere({-0.3f, 3.0f, 0.2f}, {0.75f, 0.6f, 0.7f}, shade(c, 0.92f), 10, 6, xf);
}

void addTree(r3d::Builder& b, int kind, const Mat4& xf, uint32_t seed) {
    if (kind == 0) addFir(b, xf, seed);
    else if (kind == 1) addPine(b, xf, seed);
    else addBirch(b, xf, seed);
}

// Прямая «нога/рука»: брусок, подвешенный к шарниру в начале координат (свисает вниз по -Y).
r3d::Mesh* limb(float w, float len, float d, uint32_t c, uint32_t end = 0, float endLen = 0.f) {
    r3d::Builder b;
    b.box({0, -len / 2, 0}, {w / 2, len / 2, d / 2}, c);
    if (endLen > 0) b.box({0, -len + endLen / 2 - 0.01f, 0}, {w / 2 + 0.01f, endLen / 2, d / 2 + 0.01f}, end);
    return b.build();
}

}  // namespace

// ---------- модели ----------
void Game::buildMeshes() {
    using surv::kMap;
    {   // земля: поляна, вытоптанная площадка у огня, лесная подстилка за краем — плавные пятна цвета по вершинам
        r3d::Builder b;
        const float lo = -40, hi = kMap + 40, step = 1.5f;
        const int n = (int)((hi - lo) / step) + 1;
        auto noise = [](float x, float y) {
            float v = 0, amp = 1, f = 0.09f;
            for (int o = 0; o < 3; o++) {
                float fx = x * f, fy = y * f;
                int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
                float tx = smooth01(fx - ix), ty = smooth01(fy - iy);
                auto g = [](int a, int b2) { return h01((uint32_t)(a * 73856093) ^ (uint32_t)(b2 * 19349663)); };
                float top = g(ix, iy) + (g(ix + 1, iy) - g(ix, iy)) * tx, bot = g(ix, iy + 1) + (g(ix + 1, iy + 1) - g(ix, iy + 1)) * tx;
                v += (top + (bot - top) * ty) * amp;
                amp *= 0.5f; f *= 2.1f;
            }
            return v / 1.75f;
        };
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) {
                float x = lo + i * step, y = lo + j * step;
                float nz = noise(x, y);
                float edge = std::max(std::max(-x, x - kMap), std::max(-y, y - kMap));
                float out = smooth01(edge / 4.f);
                Vec3 grass(0.40f + nz * 0.09f, 0.58f + nz * 0.08f, 0.28f + nz * 0.04f), forest(0.27f + nz * 0.06f, 0.38f + nz * 0.06f, 0.22f);
                Vec3 c = mixv(grass, forest, out);
                float fd = std::hypot(x - W.fireX, y - W.fireY);
                if (fd < 6.f) c = mixv(c, Vec3(0.6f, 0.47f, 0.33f), smooth01((6.f - fd) / 2.5f) * (0.75f + nz * 0.25f));
                b.verts.push_back({x, 0, y, 0, 127, 0, 0, rgba((int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255))});
            }
        for (int j = 0; j + 1 < n; j++)
            for (int i = 0; i + 1 < n; i++) {
                uint32_t a0 = j * n + i, a1 = a0 + 1, b0 = a0 + n, b1 = b0 + 1;
                b.idx.insert(b.idx.end(), {a0, b0, b1, a0, b1, a1});
            }
        ground = b.build();
    }
    {   // стена леса за краем поляны
        r3d::Builder b;
        for (int i = 0; i < 900; i++) {
            float a = h01(i * 3 + 1) * 2 * kPi;
            float side = h01(i * 3 + 2);
            float x, y;
            int edge = i % 4;
            float along = h01(i * 7 + 5) * (kMap + 40) - 20, depth = 1.2f + side * 26;
            if (edge == 0) { x = along; y = -depth; }
            else if (edge == 1) { x = along; y = kMap + depth; }
            else if (edge == 2) { x = -depth; y = along; }
            else { x = kMap + depth; y = along; }
            int kind = (hashU(i) % 10) < 6 ? 0 : (hashU(i) % 10) < 9 ? 1 : 2;
            float s = 0.9f + h01(i * 11) * 0.6f;
            addTree(b, kind, placeAt({x, 0, y}, a, s), i);
        }
        border = b.build();
    }
    {
        r3d::Builder b; addFir(b, Mat4::identity(), 1); fir = b.build();
        addPine(b, Mat4::identity(), 2); pine = b.build();
        addBirch(b, Mat4::identity(), 3); birch = b.build();
        b.cylinder({0, 0, 0}, 0.3f, 0.26f, 0.35f, rgba(110, 74, 46), 10);
        b.cylinder({0, 0.35f, 0}, 0.26f, 0.0f, 0.01f, rgba(214, 178, 120), 10);
        stump = b.build();
    }
    {   // кострище: камни по кругу и брёвна шалашиком
        r3d::Builder b;
        for (int i = 0; i < 10; i++) {
            float a = i * 2 * kPi / 10;
            b.sphere({std::cos(a) * 1.05f, 0.12f, std::sin(a) * 1.05f}, {0.28f, 0.2f, 0.24f}, i % 2 ? rgba(128, 128, 134) : rgba(110, 110, 118), 8, 5);
        }
        for (int i = 0; i < 5; i++) {
            float a = i * 2 * kPi / 5;
            Mat4 xf = Mat4::rotY(a) * Mat4::translate({0.55f, 0.05f, 0}) * Mat4::rotZ(1.0f);
            b.cylinder({0, 0, 0}, 0.11f, 0.09f, 0.95f, rgba(120, 78, 48), 7, xf);
        }
        b.cylinder({0, 0.01f, 0}, 0.75f, 0.75f, 0.03f, rgba(52, 40, 34), 12);
        firePit = b.build();
        // пламя: языки разной высоты и цвета вокруг сердцевины
        for (int i = 0; i < 5; i++) {
            float a = i * 2 * kPi / 5;
            b.cone({std::cos(a) * 0.22f, 0, std::sin(a) * 0.22f}, 0.24f, 0.8f + 0.25f * (i % 3), i % 2 ? rgba(240, 90, 30) : rgba(255, 130, 40), 7);
        }
        b.cone({0, 0, 0}, 0.36f, 1.45f, rgba(255, 160, 50), 8);
        b.cone({0, 0, 0}, 0.2f, 0.95f, rgba(255, 236, 140), 8);
        flame = b.build();
        b.cylinder({0, 0.04f, 0}, 0.5f, 0.45f, 0.08f, rgba(230, 70, 30), 10);
        ember = b.build();
    }
    {   // частокол: три заострённых бревна и поперечина (клетка 1×1 вдоль X)
        r3d::Builder b;
        for (int i = 0; i < 3; i++) {
            float x = -0.33f + i * 0.33f;
            uint32_t c = i == 1 ? rgba(156, 104, 62) : rgba(140, 92, 56);
            float h = 2.0f + (i == 1 ? 0.15f : 0.f);
            b.cylinder({x, 0, 0}, 0.17f, 0.17f, h, c, 8);
            b.cone({x, h, 0}, 0.17f, 0.38f, shade(c, 1.1f), 8);
        }
        b.box({0, 1.35f, -0.17f}, {0.52f, 0.08f, 0.05f}, rgba(112, 74, 44));
        b.box({0, 0.55f, -0.17f}, {0.52f, 0.08f, 0.05f}, rgba(112, 74, 44));
        palisade = b.build();
        // ворота: два столба, дверь из досок, перекладина сверху
        for (float x : {-0.43f, 0.43f}) {
            b.cylinder({x, 0, 0}, 0.13f, 0.13f, 2.5f, rgba(130, 86, 52), 8);
            b.cone({x, 2.5f, 0}, 0.13f, 0.3f, rgba(140, 94, 58), 8);
        }
        for (int i = 0; i < 3; i++) b.box({-0.2f + i * 0.2f, 0.95f, 0}, {0.095f, 0.9f, 0.06f}, i == 1 ? rgba(188, 140, 88) : rgba(176, 128, 80));
        b.box({0, 1.4f, -0.08f}, {0.32f, 0.07f, 0.04f}, rgba(96, 62, 38));
        b.box({0, 0.5f, -0.08f}, {0.32f, 0.07f, 0.04f}, rgba(96, 62, 38));
        b.box({0, 2.3f, 0}, {0.6f, 0.09f, 0.12f}, rgba(118, 78, 46));
        gate = b.build();
        // факел
        b.cylinder({0, 0, 0}, 0.07f, 0.06f, 1.7f, rgba(110, 72, 44), 7);
        b.cylinder({0, 1.65f, 0}, 0.1f, 0.16f, 0.18f, rgba(70, 64, 60), 8);
        torch = b.build();
        b.cone({0, 1.82f, 0}, 0.16f, 0.5f, rgba(255, 170, 60), 8);
        torchFlame = b.build();
        b.box({0, 1.0f, 0}, {0.5f, 1.0f, 0.18f}, rgba(255, 255, 255));
        ghost = b.build();
        b.cylinder({0, 0.02f, 0}, 1.0f, 1.0f, 0.01f, rgba(0, 0, 0), 16);
        shadow = b.build();
        b.box({0, 0, 0}, {0.5f, 0.5f, 0.5f}, rgba(255, 255, 255));
        cube = b.build();
        for (int i = 0; i < 3; i++)
            b.cylinder({0, 0, 0}, 0.09f, 0.09f, 0.7f, rgba(150, 100, 60), 7,
                       Mat4::translate({-0.32f, 0.1f + (i == 2 ? 0.15f : 0.f), (i == 2 ? 0.f : (i ? 0.1f : -0.1f))}) * Mat4::rotZ(kPi / 2) * Mat4::rotY(0));
        logPile = b.build();
    }
    {   // человек из деталей (пропорции фигурки-конструктора): ноги, туловище, руки, голова, шапка, топор
        // одежда — белая, красится цветом игрока; сапоги, кисти и кушак — отдельные детали своего цвета
        legL = limb(0.38f, 0.74f, 0.4f, rgba(255, 255, 255));
        legR = limb(0.4f, 0.18f, 0.42f, rgba(80, 58, 42));     // сапог: рисуется у конца ноги
        armL = limb(0.3f, 0.64f, 0.3f, rgba(255, 255, 255));
        armR = limb(0.26f, 0.2f, 0.26f, rgba(240, 190, 150));  // кисть
        r3d::Builder b;
        b.box({0, 0.45f, 0}, {0.24f, 0.45f, 0.44f}, rgba(255, 255, 255));
        torso = b.build();
        b.box({0, 0.12f, 0}, {0.25f, 0.06f, 0.45f}, rgba(232, 186, 64));                 // кушак
        b.box({0.245f, 0.5f, 0}, {0.01f, 0.32f, 0.05f}, rgba(232, 186, 64));             // застёжка
        belt = b.build();
        b.cylinder({0, 0, 0}, 0.31f, 0.31f, 0.52f, rgba(240, 190, 150), 16);
        b.sphere({0, 0.52f, 0}, {0.31f, 0.12f, 0.31f}, rgba(240, 190, 150), 16, 4);
        b.box({0.3f, 0.32f, -0.11f}, {0.02f, 0.05f, 0.035f}, rgba(30, 24, 22));          // глаза
        b.box({0.3f, 0.32f, 0.11f}, {0.02f, 0.05f, 0.035f}, rgba(30, 24, 22));
        b.box({0.3f, 0.15f, 0}, {0.02f, 0.02f, 0.09f}, rgba(150, 70, 60));              // рот
        b.sphere({0.25f, 0.06f, 0}, {0.12f, 0.12f, 0.2f}, rgba(150, 96, 54), 10, 6);    // борода
        head = b.build();
        b.cylinder({0, 0.48f, 0}, 0.36f, 0.35f, 0.18f, rgba(112, 80, 54), 14);         // меховой околыш
        b.sphere({0, 0.66f, 0}, {0.3f, 0.22f, 0.3f}, rgba(196, 52, 44), 14, 6);         // суконный верх
        b.sphere({0, 0.9f, 0}, {0.06f, 0.06f, 0.06f}, rgba(240, 196, 70), 8, 5);        // кисточка
        hat = b.build();
        b.cylinder({0, 0, 0}, 0.045f, 0.045f, 0.85f, rgba(150, 104, 60), 7);           // топорище вдоль +Y
        b.box({0.1f, 0.74f, 0}, {0.13f, 0.11f, 0.03f}, rgba(196, 206, 214));
        b.wedge({0.26f, 0.74f, 0}, {0.05f, 0.15f, 0.03f}, rgba(226, 232, 238), Mat4::rotZ(0));
        axe = b.build();
    }
    {   // волк
        r3d::Builder b;
        uint32_t fur = rgba(116, 116, 124), belly = rgba(170, 168, 170), dark = rgba(70, 70, 78);
        b.box({0, 0.78f, 0}, {0.55f, 0.24f, 0.22f}, fur);
        b.box({-0.05f, 0.62f, 0}, {0.42f, 0.08f, 0.18f}, belly);
        b.box({0.62f, 0.98f, 0}, {0.22f, 0.19f, 0.19f}, fur);
        b.box({0.9f, 0.9f, 0}, {0.13f, 0.09f, 0.11f}, belly);
        b.box({1.02f, 0.94f, 0}, {0.03f, 0.04f, 0.05f}, rgba(30, 26, 26));
        b.wedge({0.58f, 1.23f, -0.11f}, {0.06f, 0.08f, 0.05f}, dark);
        b.wedge({0.58f, 1.23f, 0.11f}, {0.06f, 0.08f, 0.05f}, dark);
        b.box({0.82f, 1.03f, -0.1f}, {0.02f, 0.03f, 0.03f}, rgba(250, 210, 60));
        b.box({0.82f, 1.03f, 0.1f}, {0.02f, 0.03f, 0.03f}, rgba(250, 210, 60));
        b.box({-0.72f, 0.82f, 0}, {0.26f, 0.07f, 0.07f}, dark, Mat4::translate({-0.72f, 0.82f, 0}) * Mat4::rotZ(-0.5f) * Mat4::translate({0.72f, -0.82f, 0}));
        wolfBody = b.build();
        wolfLeg = limb(0.14f, 0.62f, 0.14f, fur, dark, 0.1f);
    }
    {   // Лихо: долговязое, сгорбленное, с одним горящим глазом
        r3d::Builder b;
        uint32_t skin = rgba(136, 148, 140), rag = rgba(52, 58, 54), pale = rgba(196, 204, 192);
        Mat4 hunch = Mat4::translate({0, 1.6f, 0}) * Mat4::rotZ(-0.35f);
        b.box({0.1f, 0.6f, 0}, {0.32f, 0.65f, 0.36f}, skin, hunch);
        for (int i = 0; i < 6; i++)  // лохмотья
            b.box({-0.2f + (i % 3) * 0.2f, 0.05f, -0.3f + (i / 3) * 0.6f}, {0.07f, 0.3f, 0.04f}, rag, hunch);
        b.sphere({0.55f, 1.35f, 0}, {0.36f, 0.42f, 0.34f}, pale, 12, 8, hunch);
        b.box({0.86f, 1.18f, 0}, {0.04f, 0.04f, 0.16f}, rgba(40, 30, 30), hunch);       // рот
        b.cone({0.4f, 1.7f, -0.2f}, 0.07f, 0.3f, rag, 6, hunch);                        // космы
        b.cone({0.4f, 1.72f, 0.18f}, 0.07f, 0.34f, rag, 6, hunch);
        likhoBody = b.build();
        b.sphere({0, 0, 0}, {0.15f, 0.17f, 0.17f}, rgba(255, 230, 120), 10, 6);
        likhoEye = b.build();
        likhoArm = limb(0.16f, 1.5f, 0.16f, skin, pale, 0.25f);
        likhoLeg = limb(0.2f, 1.65f, 0.2f, skin, rag, 0.2f);
    }
}

// ---------- запуск ----------
void Game::init(uint32_t seed) {
    surv::init(W, seed ? seed : 20261029u);
    surv::addPlayer(W, "Ты", surv::Role::Carpenter);
    me = 0;
    buildMeshes();
    for (const auto& p : W.players) prevPos[p.id] = {p.x, p.y};
    yaw = std::atan2(W.fireY - W.players[0].y, W.fireX - W.players[0].x);
    audio::music("village", 0.35f);
    toast("День 1. Наруби леса и укрепи острог до ночи", rgba(255, 230, 160));
}

void Game::fastForward(float seconds) {
    int n = (int)(seconds / surv::kDt);
    for (int i = 0; i < n; i++) {
        std::vector<surv::Input> in{bot ? botInput() : surv::Input{}};
        surv::step(W, in);
    }
    for (const auto& p : W.players) prevPos[p.id] = {p.x, p.y};
    for (const auto& e : W.enemies) prevPos[e.id] = {e.x, e.y};
    toasts.clear();
}

void Game::debugSpawn(surv::EnemyKind kind, float dist) {
    const auto& p = W.players[me];
    surv::Enemy e;
    e.id = W.nextId++;
    e.kind = kind;
    float away = std::atan2(p.y - W.fireY, p.x - W.fireX);  // в темноту, от огня
    e.x = p.x + std::cos(away) * dist;
    e.y = p.y + std::sin(away) * dist;
    yaw = away;
    e.hp = e.maxHp = kind == surv::EnemyKind::Wolf ? 3.f : 28.f;
    e.facing = std::atan2(p.y - e.y, p.x - e.x);
    W.enemies.push_back(e);
    prevPos[e.id] = {e.x, e.y};
}

void Game::toast(const std::string& s, uint32_t color) { toasts.push_back({s, 0.f, color}); }

void Game::buildTarget(int& cx, int& cy) const {
    const auto& p = W.players[me];
    float d = 1.9f;
    cx = (int)std::floor(p.x + std::cos(p.facing) * d);
    cy = (int)std::floor(p.y + std::sin(p.facing) * d);
}

// ---------- ввод ----------
surv::Input Game::gatherInput() {
    surv::Input in;
    float fx = std::cos(yaw), fy = std::sin(yaw);    // «вперёд» от камеры по земле
    float rx = -std::sin(yaw), ry = std::cos(yaw);   // «вправо»
    float f = 0, r = 0;
    if (input::key(SAPP_KEYCODE_W) || input::key(SAPP_KEYCODE_UP)) f += 1;
    if (input::key(SAPP_KEYCODE_S) || input::key(SAPP_KEYCODE_DOWN)) f -= 1;
    if (input::key(SAPP_KEYCODE_D)) r += 1;
    if (input::key(SAPP_KEYCODE_A)) r -= 1;
    if (joyId >= 0) {  // экранный джойстик
        float dx = joyX - joyCX, dy = joyY - joyCY, l = std::hypot(dx, dy), maxR = 52;
        if (l > 6) { r += dx / std::max(l, maxR); f -= dy / std::max(l, maxR); }
    }
    in.mx = fx * f + rx * r;
    in.my = fy * f + ry * r;
    float l = std::hypot(in.mx, in.my);
    if (l > 1) { in.mx /= l; in.my /= l; }
    in.act = actHeld || input::key(SAPP_KEYCODE_SPACE);
    in.feed = feedTap || input::key(SAPP_KEYCODE_F);
    if (buildMode && (placeTap || input::key(SAPP_KEYCODE_E) || input::key(SAPP_KEYCODE_ENTER))) {
        in.build = true;
        in.piece = buildPiece;
        buildTarget(in.bx, in.by);
    }
    if (buildMode) in.act = false;
    feedTap = placeTap = false;
    return in;
}

// Простой помощник для автотестов: рубит, кормит огонь, ставит кольцо частокола с воротами, ночью отбивается.
surv::Input Game::botInput() {
    surv::Input in;
    const auto& p = W.players[me];
    if (!p.alive()) return in;
    auto goTo = [&](float x, float y, float stop) {
        float dx = x - p.x, dy = y - p.y, d = std::hypot(dx, dy);
        if (d > stop) { in.mx = dx / d; in.my = dy / d; }
        return d <= stop;
    };
    for (const auto& e : W.enemies)
        if (e.alive && std::hypot(e.x - p.x, e.y - p.y) < 3.f) {
            in.mx = (e.x - p.x) * 0.01f; in.my = (e.y - p.y) * 0.01f;
            in.act = true;
            return in;
        }
    if (W.night) { goTo(W.fireX + 2.5f, W.fireY, 0.5f); return in; }
    if (p.wood >= 1 && W.fuel < 110) { if (goTo(W.fireX, W.fireY, 2.6f)) in.feed = true; return in; }
    // кольцо частокола радиусом 6 клеток, ворота на юге
    if (p.wood >= 2) {
        int fx = (int)W.fireX, fy = (int)W.fireY, R = 6;
        for (int i = 0; i < 4 * 2 * R; i++) {
            int side = i / (2 * R), k = i % (2 * R) - R;
            int cx = side == 0 ? fx + k : side == 1 ? fx + R : side == 2 ? fx - k : fx - R;
            int cy = side == 0 ? fy - R : side == 1 ? fy + k : side == 2 ? fy + R : fy - k;
            if (surv::structureAt(W, cx, cy)) continue;
            bool gateCell = cx == fx && cy == fy + R;
            surv::Piece pc = gateCell ? surv::Piece::Gate : surv::Piece::Palisade;
            if (p.wood < surv::piece(pc).wood) continue;
            float tx = cx + 0.5f, ty = cy + 0.5f;
            // встать снаружи центра клетки со стороны огня
            float ox = W.fireX - tx, oy = W.fireY - ty, ol = std::hypot(ox, oy);
            if (!goTo(tx + ox / ol * 1.6f, ty + oy / ol * 1.6f, 0.4f)) return in;
            if (surv::canBuild(W, p, pc, cx, cy)) { in.build = true; in.piece = pc; in.bx = cx; in.by = cy; }
            else in.mx = (tx - p.x) * 0.02f, in.my = (ty - p.y) * 0.02f;
            return in;
        }
    }
    // рубить ближайшее дерево
    const surv::Tree* best = nullptr;
    float bd = 1e9f;
    for (const auto& t : W.trees) {
        if (!t.alive()) continue;
        float d = std::hypot(t.x - p.x, t.y - p.y);
        if (d < bd) { bd = d; best = &t; }
    }
    if (best) {
        if (goTo(best->x, best->y, 1.25f)) {
            in.mx = (best->x - p.x) * 0.02f;
            in.my = (best->y - p.y) * 0.02f;
            in.act = true;
        }
    }
    return in;
}

// ---------- симуляция и события ----------
void Game::spawnChips(float x, float y, float h, uint32_t color, int n, bool glow) {
    for (int i = 0; i < n && parts.size() < 400; i++) {
        uint32_t s = hashU((uint32_t)(t * 1000) + i * 77 + (uint32_t)parts.size());
        float a = h01(s) * 2 * kPi, sp = 1.5f + h01(s + 1) * 2.5f;
        Particle p;
        p.p = W3(x, y, h);
        p.v = {std::cos(a) * sp, 2.5f + h01(s + 2) * 3.f, std::sin(a) * sp};
        p.life = p.max = 0.6f + h01(s + 3) * 0.6f;
        p.size = 0.08f + h01(s + 4) * 0.1f;
        p.color = color;
        p.glow = glow;
        parts.push_back(p);
    }
}

void Game::handleEvents() {
    for (const auto& e : W.events) {
        bool mine = e.who == W.players[me].id;
        switch (e.type) {
            case surv::Ev::Chop: spawnChips(e.x, e.y, 1.0f, rgba(214, 168, 108), 6); audio::play("hit", 0.5f, 1.1f + h01(W.tick) * 0.2f); break;
            case surv::Ev::TreeFell:
                spawnChips(e.x, e.y, 1.5f, rgba(70, 120, 60), 14); spawnChips(e.x, e.y, 0.6f, rgba(214, 168, 108), 8);
                audio::play("destroy", 0.6f, 1.2f);
                if (mine) toast("+3 бревна", rgba(240, 210, 150));
                break;
            case surv::Ev::Hit: spawnChips(e.x, e.y, 1.0f, rgba(240, 240, 240), 5); audio::play("slash", 0.6f); break;
            case surv::Ev::PlayerHurt: if (mine) { shake = 0.3f; audio::play("hit", 0.7f, 0.7f); } break;
            case surv::Ev::PlayerDied: if (mine) { toast("Тебя повалили. Очнёшься у огня", rgba(255, 140, 120)); audio::play("lose", 0.6f); } break;
            case surv::Ev::EnemyDied: spawnChips(e.x, e.y, 0.8f, rgba(90, 90, 96), 12); audio::play("done", 0.5f, 0.8f); break;
            case surv::Ev::Built: spawnChips(e.x, e.y, 0.3f, rgba(190, 160, 120), 8); audio::play("build", 0.6f); break;
            case surv::Ev::Broken: spawnChips(e.x, e.y, 1.0f, rgba(150, 100, 60), 14); audio::play("destroy", 0.7f); toast("Пролом в частоколе!", rgba(255, 150, 120)); break;
            case surv::Ev::Fed: spawnChips(e.x, e.y, 1.2f, rgba(255, 190, 80), 10, true); audio::play("flame", 0.4f); break;
            case surv::Ev::NightBegins:
                toast("Ночь " + std::to_string(W.day) + ". Держитесь у огня!", rgba(170, 190, 255));
                audio::play("wave", 0.8f); audio::music("battle", 0.4f);
                break;
            case surv::Ev::DayBegins:
                toast("Рассвет! Пережили ночь " + std::to_string(W.day - 1), rgba(255, 230, 160));
                audio::play("win", 0.7f); audio::music("village", 0.35f);
                break;
            case surv::Ev::LikhoComes: toast("Из чащи идёт Лихо. Оно боится огня", rgba(255, 120, 110)); audio::play("eclipse", 0.9f); break;
            case surv::Ev::FireOut: toast("Огонь погас! Подкинь брёвен", rgba(255, 120, 110)); audio::play("lose", 0.6f, 0.8f); break;
        }
    }
}

void Game::stepSim(float dt) {
    acc += dt;
    int guard = 0;
    while (acc >= surv::kDt && guard++ < 5) {
        acc -= surv::kDt;
        for (const auto& p : W.players) prevPos[p.id] = {p.x, p.y};
        for (const auto& e : W.enemies) prevPos[e.id] = {e.x, e.y};
        std::vector<surv::Input> in{bot ? botInput() : gatherInput()};
        surv::step(W, in);
        handleEvents();
        for (const auto& p : W.players) {
            float sp = std::hypot(p.vx, p.vy);
            walkPhase[p.id] += sp * surv::kDt * 1.9f;
        }
        for (const auto& e : W.enemies) walkPhase[e.id] += (e.kind == surv::EnemyKind::Wolf ? 4.3f : 2.6f) * surv::kDt * 1.6f;
    }
    if (acc > surv::kDt) acc = 0;
}

Vec3 Game::lerpPos(int id, float x, float y, float alpha) {
    auto it = prevPos.find(id);
    if (it == prevPos.end()) return W3(x, y);
    return W3(it->second.x + (x - it->second.x) * alpha, it->second.y + (y - it->second.y) * alpha);
}

// ---------- свет по времени суток ----------
r3d::Lighting Game::lighting() const {
    r3d::Lighting day, dusk, night;
    day.sunDir = {-0.45f, -1.f, -0.25f};
    day.sunColor = {0.78f, 0.74f, 0.64f};
    day.skyColor = {0.46f, 0.52f, 0.62f};
    day.groundColor = {0.30f, 0.27f, 0.21f};
    day.fogColor = {0.72f, 0.80f, 0.86f};
    day.fogNear = 35; day.fogFar = 95;
    dusk = day;
    dusk.sunDir = {-0.8f, -0.45f, -0.2f};
    dusk.sunColor = {1.0f, 0.58f, 0.32f};
    dusk.skyColor = {0.5f, 0.42f, 0.5f};
    dusk.fogColor = {0.78f, 0.56f, 0.48f};
    dusk.fogNear = 25; dusk.fogFar = 80;
    night = day;
    night.sunDir = {0.3f, -1.f, 0.4f};
    night.sunColor = {0.22f, 0.27f, 0.46f};
    night.skyColor = {0.13f, 0.15f, 0.27f};
    night.groundColor = {0.04f, 0.04f, 0.07f};
    night.fogColor = {0.05f, 0.06f, 0.12f};
    night.fogNear = 14; night.fogFar = 48;
    auto mix = [](const r3d::Lighting& a, const r3d::Lighting& b, float k) {
        r3d::Lighting r = a;
        r.sunDir = mixv(a.sunDir, b.sunDir, k);
        r.sunColor = mixv(a.sunColor, b.sunColor, k);
        r.skyColor = mixv(a.skyColor, b.skyColor, k);
        r.groundColor = mixv(a.groundColor, b.groundColor, k);
        r.fogColor = mixv(a.fogColor, b.fogColor, k);
        r.fogNear = a.fogNear + (b.fogNear - a.fogNear) * k;
        r.fogFar = a.fogFar + (b.fogFar - a.fogFar) * k;
        return r;
    };
    r3d::Lighting L;
    float left = surv::phaseLeft(W);
    if (!W.night) {
        float dawn = W.day == 1 ? 1.f : smooth01(W.phaseT / 12.f);  // первые секунды дня — рассвет (кроме самого первого)
        float eve = smooth01((25.f - left) / 25.f);               // последние 25 секунд — закат
        L = mix(mix(night, day, dawn), dusk, eve);
        if (left < 6.f) L = mix(L, night, smooth01((6.f - left) / 6.f));
    } else {
        L = night;
    }
    float r = surv::fireRadius(W);
    float flick = 0.9f + 0.1f * std::sin(t * 13.f) * std::sin(t * 7.3f + 1.f);
    float nightK = W.night ? 1.f : smooth01((25.f - left) / 25.f) * 0.6f;
    if (r > 0) L.lights[0] = {W3(W.fireX, W.fireY, 1.4f), (r * 1.5f + 3.f) * flick, {1.0f, 0.55f, 0.22f}, 0.4f + 1.6f * nightK};
    // ближайшие к игроку факелы
    const auto& me_ = W.players[me];
    std::vector<std::pair<float, const surv::Structure*>> lit;
    for (const auto& s : W.structures)
        if (s.kind == surv::Piece::Torch) lit.push_back({std::hypot(s.cx + 0.5f - me_.x, s.cy + 0.5f - me_.y), &s});
    std::sort(lit.begin(), lit.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (size_t i = 0; i < lit.size() && i < 3; i++)
        L.lights[1 + i] = {W3(lit[i].second->cx + 0.5f, lit[i].second->cy + 0.5f, 2.f), 6.5f * flick, {1.0f, 0.62f, 0.3f}, 0.3f + 1.3f * nightK};
    return L;
}

// ---------- отрисовка мира ----------
void Game::drawPlayer(const surv::Player& p, float alpha) {
    if (!p.alive()) return;
    Vec3 pos = lerpPos(p.id, p.x, p.y, alpha);
    float ph = walkPhase[p.id];
    float moving = std::min(1.f, std::hypot(p.vx, p.vy) / 4.f);
    float bobY = std::fabs(std::sin(ph)) * 0.08f * moving;
    Mat4 root = Mat4::translate(pos + Vec3(0, bobY, 0)) * Mat4::rotY(-p.facing);
    static const uint32_t shirts[6] = {rgba(196, 58, 48), rgba(56, 104, 190), rgba(70, 150, 76), rgba(220, 160, 50), rgba(140, 70, 160), rgba(60, 160, 170)};
    uint32_t shirt = shirts[(p.id) % 6], pants = rgba(64, 70, 100);
    if (p.hurtT > 0) { shirt = rgba(255, 170, 160); pants = rgba(255, 170, 160); }
    r3d::draw(shadow, Mat4::translate(pos + Vec3(0, 0.02f, 0)) * Mat4::scale({0.62f, 1, 0.62f}), rgba(0, 0, 0, 80));
    float sw = std::sin(ph) * 0.65f * moving;
    for (int side = -1; side <= 1; side += 2) {  // нога: штанина и сапог
        Mat4 hip = root * Mat4::translate({0, 0.9f, side * 0.2f}) * Mat4::rotZ(side < 0 ? sw : -sw);
        r3d::draw(legL, hip, pants);
        r3d::draw(legR, hip * Mat4::translate({0, -0.72f, 0}));
    }
    r3d::draw(torso, root * Mat4::translate({0, 0.9f, 0}), shirt);
    r3d::draw(belt, root * Mat4::translate({0, 0.9f, 0}));
    float armL_ = -sw * 0.9f, armR_ = sw * 0.9f;
    if (p.swing > 0) {
        float u = 1 - p.swing / 0.35f;
        armR_ = 2.9f + (0.3f - 2.9f) * smooth01(u * 1.3f);
    }
    Mat4 shR = root * Mat4::translate({0, 1.72f, 0.6f}) * Mat4::rotZ(armR_) * Mat4::rotX(-0.08f);
    Mat4 shL = root * Mat4::translate({0, 1.72f, -0.6f}) * Mat4::rotZ(armL_) * Mat4::rotX(0.08f);
    r3d::draw(armL, shL, shirt);
    r3d::draw(armL, shR, shirt);
    r3d::draw(armR, shL * Mat4::translate({0, -0.62f, 0}));
    r3d::draw(armR, shR * Mat4::translate({0, -0.62f, 0}));
    r3d::draw(axe, shR * Mat4::translate({0, -0.72f, 0}) * Mat4::rotZ(-kPi / 2 + 0.25f));
    Mat4 hd = root * Mat4::translate({0, 1.8f, 0});
    r3d::draw(head, hd);
    r3d::draw(hat, hd);
    if (p.wood > 0) {  // брёвна за спиной
        int n = std::min(3, (p.wood + 4) / 5);
        for (int i = 0; i < n; i++) r3d::draw(logPile, root * Mat4::translate({-0.38f, 1.0f + i * 0.22f, 0}) * Mat4::rotY(kPi / 2) * Mat4::scale({0.9f, 0.9f, 0.9f}));
    }
}

void Game::drawEnemy(const surv::Enemy& e, float alpha) {
    Vec3 pos = lerpPos(e.id, e.x, e.y, alpha);
    float fade = e.alive ? 1.f : std::max(0.f, 1.f - e.deadT / 1.2f);
    if (fade <= 0) return;
    float sink = e.alive ? 0.f : smooth01(e.deadT / 1.2f) * 1.2f;
    uint32_t tint = e.hurtT > 0 ? rgba(255, 150, 140) : rgba(255, 255, 255);
    if (!e.alive) tint = rgba(255, 255, 255, (int)(255 * fade));
    float ph = walkPhase[e.id];
    float lunge = e.attackT > 0 ? std::sin(e.attackT / 0.3f * kPi) * 0.35f : 0.f;
    Mat4 root = Mat4::translate(pos - Vec3(0, sink, 0)) * Mat4::rotY(-e.facing) * Mat4::translate({lunge, 0, 0});
    if (e.kind == surv::EnemyKind::Wolf) {
        r3d::draw(shadow, Mat4::translate(pos + Vec3(0, 0.02f, 0)) * Mat4::scale({0.8f, 1, 0.45f}), rgba(0, 0, 0, (int)(70 * fade)));
        r3d::draw(wolfBody, root * Mat4::translate({0, std::fabs(std::sin(ph)) * 0.06f, 0}), tint);
        float s = std::sin(ph) * 0.7f;
        float xs[2] = {0.38f, -0.38f}, zs[2] = {-0.14f, 0.14f};
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                r3d::draw(wolfLeg, root * Mat4::translate({xs[i], 0.62f, zs[j]}) * Mat4::rotZ((i ^ j) ? s : -s), tint);
    } else {
        r3d::draw(shadow, Mat4::translate(pos + Vec3(0, 0.02f, 0)) * Mat4::scale({1.1f, 1, 0.9f}), rgba(0, 0, 0, (int)(90 * fade)));
        float s = std::sin(ph) * 0.45f;
        r3d::draw(likhoLeg, root * Mat4::translate({0, 1.65f, -0.22f}) * Mat4::rotZ(s), tint);
        r3d::draw(likhoLeg, root * Mat4::translate({0, 1.65f, 0.22f}) * Mat4::rotZ(-s), tint);
        r3d::draw(likhoBody, root * Mat4::translate({0, 0.05f, 0}), tint, 0.12f);  // бледное, чуть светится в темноте
        float reach = e.attackT > 0 ? 1.4f : 0.25f;
        Mat4 hunch = root * Mat4::translate({0, 1.65f, 0}) * Mat4::rotZ(-0.35f);
        r3d::draw(likhoArm, hunch * Mat4::translate({0.2f, 1.1f, -0.42f}) * Mat4::rotZ(reach - s * 0.6f), tint);
        r3d::draw(likhoArm, hunch * Mat4::translate({0.2f, 1.1f, 0.42f}) * Mat4::rotZ(reach + s * 0.6f), tint);
        r3d::draw(likhoEye, hunch * Mat4::translate({0.86f, 1.42f, 0}), rgba(255, 255, 255, (int)(255 * fade)), 1.f);
    }
}

void Game::drawWorld(float alpha) {
    r3d::draw(ground, Mat4::identity());
    r3d::draw(border, Mat4::identity());
    for (const auto& tr : W.trees) {
        Mat4 base = Mat4::translate(W3(tr.x, tr.y));
        if (!tr.alive()) { r3d::draw(stump, base); continue; }
        float s = 0.9f + h01(tr.id) * 0.35f;
        float wob = tr.shake > 0 ? std::sin(tr.shake * 60) * tr.shake * 0.25f : 0.f;
        Mat4 xf = base * Mat4::rotY(h01(tr.id * 7) * 6.28f) * Mat4::rotZ(wob) * Mat4::scale({s, s, s});
        r3d::draw(tr.kind == 0 ? fir : tr.kind == 1 ? pine : birch, xf);
    }
    // огонь
    Mat4 fp = Mat4::translate(W3(W.fireX, W.fireY));
    r3d::draw(firePit, fp);
    if (W.fuel > 0) {
        float k = std::clamp(W.fuel / 120.f, 0.7f, 1.1f) * (0.9f + 0.15f * W.fireLevel);
        if (h01(W.tick * 13 + (uint32_t)(t * 60)) < 0.12f && parts.size() < 380) {  // искры
            Particle sp;
            sp.p = W3(W.fireX + (h01(W.tick) - 0.5f) * 0.6f, W.fireY + (h01(W.tick + 9) - 0.5f) * 0.6f, 1.0f);
            sp.v = {(h01(W.tick + 3) - 0.5f) * 0.8f, 2.5f + h01(W.tick + 5) * 2.f, (h01(W.tick + 7) - 0.5f) * 0.8f};
            sp.life = sp.max = 0.9f;
            sp.size = 0.06f;
            sp.color = rgba(255, 190, 80);
            sp.glow = true;
            parts.push_back(sp);
        }
        float fl = 1.f + 0.12f * std::sin(t * 17) + 0.08f * std::sin(t * 29 + 1);
        r3d::draw(flame, fp * Mat4::translate({0, 0.15f, 0}) * Mat4::rotY(t * 2.f) * Mat4::scale({k, k * fl, k}), rgba(255, 255, 255), 1.f);
        r3d::draw(flame, fp * Mat4::translate({0.12f, 0.15f, -0.1f}) * Mat4::rotY(-t * 3.f) * Mat4::scale({k * 0.6f, k * fl * 0.8f, k * 0.6f}),
                  rgba(255, 255, 255), 1.f);
    } else {
        r3d::draw(ember, fp, rgba(255, 255, 255), 0.6f + 0.2f * std::sin(t * 3));
    }
    // постройки
    for (const auto& s : W.structures) {
        Vec3 c = W3(s.cx + 0.5f, s.cy + 0.5f);
        float jig = s.hitT > 0 ? std::sin(s.hitT * 80) * 0.04f : 0.f;
        uint32_t tint = s.hp < s.maxHp * 0.5f ? rgba(200, 180, 170) : rgba(255, 255, 255);
        if (s.kind == surv::Piece::Torch) {
            r3d::draw(torch, Mat4::translate(c));
            float fl = 1.f + 0.15f * std::sin(t * 19 + s.id);
            r3d::draw(torchFlame, Mat4::translate(c) * Mat4::translate({0, 1.82f, 0}) * Mat4::scale({1, fl, 1}) * Mat4::translate({0, -1.82f, 0}),
                      rgba(255, 255, 255), 1.f);
            continue;
        }
        // поворот звена: вдоль соседей
        bool nx = surv::structureAt(W, s.cx - 1, s.cy) || surv::structureAt(W, s.cx + 1, s.cy);
        bool ny = surv::structureAt(W, s.cx, s.cy - 1) || surv::structureAt(W, s.cx, s.cy + 1);
        float rot = (ny && !nx) ? kPi / 2 : 0.f;
        r3d::draw(s.kind == surv::Piece::Gate ? gate : palisade, Mat4::translate(c + Vec3(jig, 0, 0)) * Mat4::rotY(rot), tint);
    }
    // предпросмотр стройки
    if (buildMode && W.players[me].alive()) {
        int cx, cy;
        buildTarget(cx, cy);
        bool ok = surv::canBuild(W, W.players[me], buildPiece, cx, cy);
        uint32_t tint = ok ? rgba(90, 255, 120, 170) : rgba(255, 60, 50, 180);
        float h = buildPiece == surv::Piece::Torch ? 0.9f : buildPiece == surv::Piece::Gate ? 1.25f : 1.1f;
        float w = buildPiece == surv::Piece::Torch ? 0.25f : 1.f;
        r3d::draw(ghost, Mat4::translate(W3(cx + 0.5f, cy + 0.5f)) * Mat4::scale({w, h, buildPiece == surv::Piece::Torch ? 1.2f : 1.6f}), tint, 0.7f);
    }
    for (const auto& p : W.players) drawPlayer(p, alpha);
    for (const auto& e : W.enemies) drawEnemy(e, alpha);
    for (const auto& p : parts) {
        float k = p.life / p.max;
        r3d::draw(cube, Mat4::translate(p.p) * Mat4::rotY(p.life * 7) * Mat4::scale({p.size, p.size, p.size}), gfx::withAlpha(p.color, std::min(1.f, k * 2)),
                  p.glow ? 1.f : 0.f);
    }
}

// ---------- интерфейс ----------
void Game::drawTouchControls() {
    float W_ = W2, H_ = H2;
    // джойстик
    float jx = joyId >= 0 ? joyCX : 110, jy = joyId >= 0 ? joyCY : H_ - 110;
    atlas::drawRect(atlas::get("fx/dot"), jx - 58, jy - 58, 116, 116, rgba(10, 16, 30, 90));
    float kx = jx, ky = jy;
    if (joyId >= 0) {
        float dx = joyX - joyCX, dy = joyY - joyCY, l = std::hypot(dx, dy);
        if (l > 52) { dx *= 52 / l; dy *= 52 / l; }
        kx += dx; ky += dy;
    }
    atlas::drawRect(atlas::get("ui/round"), kx - 30, ky - 30, 60, 60, rgba(255, 255, 255, 210));
    (void)W_;
}

void Game::drawHud(float dt) {
    float W_ = W2, H_ = H2;
    const auto& p = W.players[me];
    const uint32_t ink = rgba(246, 240, 230), soft = rgba(196, 204, 220);
    // время суток
    {
        ui::Rect r{16, 12, 250, 70};
        ui::panel(r, ui::Style::Dark);
        std::string title = (W.night ? "Ночь " : "День ") + std::to_string(W.day);
        font::draw(title, r.x + 52, r.y + 8, 22, W.night ? rgba(180, 200, 255) : rgba(255, 220, 130));
        ui::icon("icon/clock", r.x + 26, r.y + 26, 34);
        int left = (int)surv::phaseLeft(W);
        char buf[48];
        std::snprintf(buf, sizeof buf, W.night ? "до рассвета %d:%02d" : "до ночи %d:%02d", left / 60, left % 60);
        font::draw(buf, r.x + 52, r.y + 36, 15, soft);
        float frac = surv::phaseLeft(W) / (W.night ? surv::kNightLen : surv::kDayLen);
        ui::bar({r.x + 14, r.y + 56, r.w - 28, 8}, frac, W.night ? rgba(120, 150, 255) : rgba(255, 196, 80));
    }
    // здоровье
    {
        ui::Rect r{16, 90, 250, 40};
        ui::panel(r, ui::Style::Dark);
        ui::icon("icon/heart", r.x + 22, r.y + 20, 28);
        ui::bar({r.x + 44, r.y + 14, r.w - 60, 12}, p.hp / p.maxHp, rgba(230, 70, 60));
    }
    // брёвна
    {
        ui::Rect r{W_ - 186, 12, 170, 50};
        ui::panel(r, ui::Style::Dark);
        ui::icon("icon/ore", r.x + 28, r.y + 25, 40);
        font::draw(std::to_string(p.wood) + " / 30", r.x + 56, r.y + 12, 22, ink);
    }
    // огонь
    {
        float fw = 260, fx = W_ / 2 - fw / 2;
        ui::Rect r{fx, 12, fw, 46};
        ui::panel(r, ui::Style::Dark);
        float fuel = W.fuel / 240.f;
        uint32_t col = W.fuel < 30 ? rgba(255, 90, 70) : rgba(255, 160, 60);
        font::draw("Огонь · ур. " + std::to_string(W.fireLevel), r.x + 14, r.y + 6, 15, W.fuel > 0 ? rgba(255, 200, 120) : rgba(255, 110, 100));
        ui::bar({r.x + 14, r.y + 28, r.w - 28, 9}, fuel, col);
    }
    // подсказка рядом с огнём
    bool nearFire = std::hypot(p.x - W.fireX, p.y - W.fireY) < 3.5f;
    // кнопки действий (справа снизу)
    float bx = W_ - 90, by = H_ - 92;
    if (!buildMode) {
        if (ui::roundButton(bx, by, 52, surv::treeInReach(W, p) ? "icon/builder" : "icon/night", "Рубить", ui::Style::Gold)) {}
        if (ui::roundButton(bx - 118, by + 18, 36, "icon/build", "Строить", ui::Style::Green)) { buildMode = true; audio::play("open", 0.5f); }
        if (nearFire && p.wood > 0 && ui::roundButton(bx - 32, by - 118, 36, "icon/ore", "В огонь", ui::Style::Red)) feedTap = true;
    } else {
        const surv::Piece list[3] = {surv::Piece::Palisade, surv::Piece::Gate, surv::Piece::Torch};
        float x0 = W_ - 3 * 132 - 20;
        for (int i = 0; i < 3; i++) {
            const auto& d = surv::piece(list[i]);
            ui::Rect r{x0 + i * 132, H_ - 196, 122, 60};
            bool sel = buildPiece == list[i];
            if (ui::button(r, std::string(d.name) + " · " + std::to_string(d.wood), sel ? ui::Style::Gold : ui::Style::Dark, true, 15)) buildPiece = list[i];
        }
        int cx, cy;
        buildTarget(cx, cy);
        std::string why;
        bool ok = surv::canBuild(W, p, buildPiece, cx, cy, &why);
        if (ui::roundButton(bx, by, 52, "icon/upgrade", "Поставить", ui::Style::Green, ok)) placeTap = true;
        if (ui::roundButton(bx - 118, by + 18, 36, "icon/close", "Готово", ui::Style::Red)) { buildMode = false; audio::play("close", 0.5f); }
        if (!ok) font::draw(why, W_ / 2, H_ - 120, 18, rgba(255, 170, 150), font::Center);
    }
    if (touch) drawTouchControls();
    // погиб
    if (!p.alive()) {
        gfx::fillRect(0, 0, W_, H_, rgba(20, 0, 0, 90));
        font::draw("Очнёшься у огня через " + std::to_string((int)std::ceil(p.respawn)), W_ / 2, H_ / 2 - 20, 28, ink, font::Center);
    }
    // всплывающие сообщения
    float ty = H_ * 0.26f;
    for (auto it = toasts.begin(); it != toasts.end();) {
        it->t += dt;
        float a = it->t < 0.25f ? it->t / 0.25f : it->t > 2.8f ? std::max(0.f, 1 - (it->t - 2.8f) / 0.5f) : 1.f;
        if (it->t > 3.3f) { it = toasts.erase(it); continue; }
        float w = font::measure(it->text, 22) + 44;
        atlas::draw9(atlas::get("ui/panel_dark"), W_ / 2 - w / 2, ty, w, 44, 70, 18, rgba(255, 255, 255, (int)(230 * a)));
        font::draw(it->text, W_ / 2, ty + 10, 22, gfx::withAlpha(it->color, a), font::Center);
        ty += 52;
        ++it;
    }
    // подсказка управления на ПК
    if (!touch)
        font::draw("WASD — идти · пробел — рубить/бить · F — в огонь · B — строить · 1/2/3 — деталь · E — поставить · Q, мышь — камера", 16,
                   H_ - 26, 13, rgba(255, 255, 255, 140));
}

// ---------- кадр ----------
void Game::frame(float dt, float width, float height) {
    t += dt;
    const float k = kVirtualH / height;
    W2 = width * k;
    H2 = kVirtualH;
    auto events = input::drain();
    for (auto& e : events) { e.x *= k; e.y *= k; }
    // касания мира: левая треть — джойстик, остальное — поворот камеры (кнопки забирает интерфейс)
    std::vector<input::Event> forUi;
    for (const auto& e : events) {
        bool onButtons = e.x > W2 - 420 && e.y > H2 - 220;
        if (e.type == input::Type::Down) {
            if (e.id != 0) touch = true;
            if (e.x < W2 * 0.35f && e.y > H2 * 0.3f && joyId < 0) { joyId = e.id; joyCX = joyX = e.x; joyCY = joyY = e.y; continue; }
            if (!onButtons && e.y > 140 && lookId < 0) { lookId = e.id; lookX = e.x; continue; }
        } else if (e.type == input::Type::Move) {
            if (e.id == joyId) { joyX = e.x; joyY = e.y; continue; }
            if (e.id == lookId) { yaw += (e.x - lookX) * 0.008f; lookX = e.x; continue; }
        } else if (e.type == input::Type::Up || e.type == input::Type::Cancel) {
            if (e.id == joyId) { joyId = -1; continue; }
            if (e.id == lookId) { lookId = -1; continue; }
        }
        forUi.push_back(e);
    }
    // удержание кнопки «Рубить»: палец внутри круга
    actHeld = false;
    ui::beginFrame(forUi);
    gfx::beginFrame(W2, H2);
    ui::setTime(t);

    // клавиши: камера, режимы
    static bool bPrev = false, oneP = false, twoP = false, threeP = false;
    if (input::key(SAPP_KEYCODE_Q)) yaw -= 1.8f * dt;
    if (input::key(SAPP_KEYCODE_LEFT)) yaw -= 1.8f * dt;
    if (input::key(SAPP_KEYCODE_RIGHT)) yaw += 1.8f * dt;
    bool b = input::key(SAPP_KEYCODE_B);
    if (b && !bPrev) buildMode = !buildMode;
    bPrev = b;
    bool one = input::key(SAPP_KEYCODE_1), two = input::key(SAPP_KEYCODE_2), three = input::key(SAPP_KEYCODE_3);
    if (one && !oneP) buildPiece = surv::Piece::Palisade;
    if (two && !twoP) buildPiece = surv::Piece::Gate;
    if (three && !threeP) buildPiece = surv::Piece::Torch;
    oneP = one; twoP = two; threeP = three;
    // кнопка «Рубить» удерживается пальцем — проверяем все активные касания по её кругу
    {
        float bx = W2 - 90, by = H2 - 92;
        for (const auto& e : events)
            if ((e.type == input::Type::Down || e.type == input::Type::Move) && std::hypot(e.x - bx, e.y - by) < 56 && !buildMode) actId = e.id;
            else if ((e.type == input::Type::Up || e.type == input::Type::Cancel) && e.id == actId) actId = -1;
        actHeld = actId >= 0;
    }

    stepSim(dt);
    shake = std::max(0.f, shake - dt);
    for (auto& p : parts) { p.life -= dt; p.v.y -= 9.f * dt; p.p = p.p + p.v * dt; if (p.p.y < 0.05f) { p.p.y = 0.05f; p.v = p.v * 0.4f; } }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());

    // камера за спиной
    float alpha = acc / surv::kDt;
    const auto& p = W.players[me];
    Vec3 focus = lerpPos(p.id, p.x, p.y, alpha) + Vec3(0, 1.4f, 0);
    if (!p.alive()) focus = W3(W.fireX, W.fireY, 1.4f);
    r3d::Camera cam;
    float sx = shake > 0 ? std::sin(t * 70) * shake * 0.3f : 0.f;
    cam.target = focus + Vec3(sx, 0, 0);
    cam.eye = focus + Vec3(-std::cos(yaw) * std::cos(pitch) * camDist, std::sin(pitch) * camDist, -std::sin(yaw) * std::cos(pitch) * camDist);
    cam.fovY = 0.95f;
    r3d::Lighting L = lighting();
    r3d::beginFrame(cam, L, W2, H2);
    drawWorld(alpha);
    drawHud(dt);
    ui::endFrame();
    uint32_t clear = rgba((int)(L.fogColor.x * 255), (int)(L.fogColor.y * 255), (int)(L.fogColor.z * 255));
    gfx::endFrame(clear);
    audio::update();
}

}  // namespace sclient
