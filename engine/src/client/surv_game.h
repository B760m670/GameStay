#pragma once
// Клиент «Острога: 99 ночей тайги»: 3D-мир из деталей, свой персонаж, камера за спиной, управление
// (клавиатура или касания: джойстик слева, кнопки справа), интерфейс дня и ночи. Симуляция — game/surv.
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/input.h"
#include "engine/r3d.h"
#include "game/surv.h"

namespace sclient {

struct Particle {
    r3d::Vec3 p, v;
    float life, max, size;
    uint32_t color;
    bool glow = false;
};

struct Toast {
    std::string text;
    float t;
    uint32_t color;
};

class Game {
public:
    void init(uint32_t seed = 0);
    void frame(float dt, float width, float height);
    // Для автотестов и показа: бот ведёт игрока; перемотка симуляции вперёд.
    bool bot = false;
    void fastForward(float seconds);
    void setCameraYaw(float a) { yaw = a; }
    void setBuildMode(bool on) { buildMode = on; }
    // Для показа и автотестов: поставить врага в dist метрах перед игроком.
    void debugSpawn(surv::EnemyKind kind, float dist);

private:
    surv::World W;
    int me = 0;  // индекс своего игрока
    float acc = 0, t = 0;
    float yaw = 0.9f, pitch = 0.62f, camDist = 12.5f;
    bool buildMode = false;
    surv::Piece buildPiece = surv::Piece::Palisade;
    // сглаживание: позиции прошлого такта
    struct Prev { float x, y; };
    std::unordered_map<int, Prev> prevPos;
    std::unordered_map<int, float> walkPhase;
    std::vector<Particle> parts;
    std::vector<Toast> toasts;
    float shake = 0;

    // управление касаниями
    int joyId = -1, lookId = -1, actId = -1;
    float joyX = 0, joyY = 0, joyCX = 0, joyCY = 0, lookX = 0;
    bool actHeld = false, feedTap = false, placeTap = false;
    float W2 = 1, H2 = 1;  // размер кадра в единицах вёрстки
public:
    bool touch = false;    // сенсорный экран: показывать джойстик (на iOS — сразу)
private:

    // модели
    r3d::Mesh *ground = nullptr, *border = nullptr, *fir = nullptr, *pine = nullptr, *birch = nullptr, *stump = nullptr;
    r3d::Mesh *firePit = nullptr, *flame = nullptr, *ember = nullptr, *palisade = nullptr, *gate = nullptr, *torch = nullptr;
    r3d::Mesh *torchFlame = nullptr, *ghost = nullptr, *shadow = nullptr, *cube = nullptr, *logPile = nullptr;
    r3d::Mesh *head = nullptr, *torso = nullptr, *belt = nullptr, *armL = nullptr, *armR = nullptr, *legL = nullptr, *legR = nullptr, *hat = nullptr, *axe = nullptr;
    r3d::Mesh *wolfBody = nullptr, *wolfLeg = nullptr, *likhoBody = nullptr, *likhoArm = nullptr, *likhoLeg = nullptr, *likhoEye = nullptr;

    void buildMeshes();
    surv::Input gatherInput();
    surv::Input botInput();
    void stepSim(float dt);
    void handleEvents();
    void spawnChips(float x, float y, float h, uint32_t color, int n, bool glow = false);
    void toast(const std::string& s, uint32_t color);
    r3d::Lighting lighting() const;
    void drawWorld(float alpha);
    void drawPlayer(const surv::Player& p, float alpha);
    void drawEnemy(const surv::Enemy& e, float alpha);
    void drawHud(float dt);
    void drawTouchControls();
    void buildTarget(int& cx, int& cy) const;
    r3d::Vec3 lerpPos(int id, float x, float y, float alpha);
};

}  // namespace sclient
