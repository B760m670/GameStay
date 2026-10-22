#pragma once
// Клиент Lanternfall: сцены деревни и боя, модальные окна, диалоги, сохранения.
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "camera.h"
#include "world.h"
#include "engine/gfx.h"
#include "engine/input.h"
#include "engine/ui.h"
#include "game/core.h"

namespace client {

enum class Scene { Village, Battle };
enum class Modal { None, Shop, Army, Map, Night, Codex, Menu, Result, Info, Tasks, Workshop };
enum class BattleKind { Raid, Mission, Night };

struct Toast { std::string text; float t; uint32_t color; };

struct Placing {
    bool active = false;
    lf::BType type = lf::BType::Wall;
    int x = 0, y = 0;
    int moveId = -1;   // >=0 — перенос существующей постройки
    bool valid = false;
    std::string reason;
    int dirX = 1, dirY = 0;
    float grabX = 0, grabY = 0;
    bool dragging = false;
};

struct BattleState {
    std::unique_ptr<lf::Battle> b;
    BattleKind kind = BattleKind::Raid;
    int missionIdx = -1;
    std::string title;
    bool started = false;
    float scout = 30, acc = 0, holdT = 0, endDelay = 0;
    int speed = 1;
    int sel = -1;                 // индекс типа войск, 100 — Затмение
    bool holding = false;
    Vec2 holdPos{0, 0};
    std::vector<std::pair<int, float>> flashes;  // клетка, время
    float warnT = 0;
    bool resultShown = false;
    // зрелищность: время с гибели юнитов и построек, тряска камеры, обломки, анимация итогов
    std::vector<float> troopDeadT, bDeadT;
    float shake = 0, resultT = 0;
    int starsShown = 0;
    std::vector<world::Particle> parts;
    // итог
    lf::BattleResult res;
    long long gotOre = 0, gotAether = 0, gotShards = 0, lostOre = 0, lostAether = 0;
    bool firstWin = false;
    int ratingDelta = 0;
    lf::replay::Record rec;  // запись для продолжения после закрытия приложения
};

class App {
public:
    void init();
    void frame(float dt, float width, float height);
    void save();
    lf::VillageState& state() { return S; }

    // для автотестов
    void startMission(int idx);
    void startRaid();
    void startNight();
    void skipDialogue() { dlg.clear(); }
    void viewRadius(float r) { requestFocus(22, 22, r, 2.f); }
    void autoDeploy();
    void selectType(const std::string& id) {
        for (auto& b : S.buildings) if (std::string(lf::B(b.type).id) == id) { selected = b.id; return; }
    }
    Scene scene = Scene::Village;
    Modal modal = Modal::None;
    bool title = true;  // титульный экран при запуске

private:
    // общее
    lf::VillageState S;
    Camera cam;
    Gestures gest;
    float t = 0, tickT = 0, saveT = 0;
    float safeX = 12, safeB = 10;
    struct Focus { bool pending = false; float x = 22, y = 22, r = 10, maxZ = 1.2f; } focus;
    void requestFocus(float x, float y, float r, float maxZ) { focus = {true, x, y, r, maxZ}; }
    std::vector<Toast> toasts;
    void toast(const std::string& s, uint32_t color = 0xffffffffu);
    void drawToasts(float dt);
    long long now() const;

    // анимации интерфейса
    Modal shownModal = Modal::None;
    float modalT = 0;
    struct Flyer { lf::Res res; Vec2 from, to; float t, delay; long long amount; };
    std::vector<Flyer> flyers;
    double shownOre = -1, shownAether = -1, shownShards = -1;
    Vec2 resAnchor[3]{};
    void spawnFlyers(lf::Res r, Vec2 from, long long amount);
    void drawFlyers(float dt);
    void drawInfo();
    void drawTasks();
    // жизнь деревни
    struct Actor { std::string unit; float x, y, tx, ty, angle = 0, wait = 0, anim = 0; int job = -1; bool working = false; };
    std::vector<Actor> workers, idlers;
    Actor heroA{"hero", 0, 0, 0, 0};
    bool heroOn = false;
    std::vector<world::Particle> particles;
    float emitT = 0;
    void updateLife(float dt);
    void drawWorkshop();

    // титульный экран
    const gfx::Texture* splash = nullptr;
    float titleOut = -1;  // >= 0 — идёт растворение заставки
    std::vector<world::Particle> embers;
    void drawTitle(float dt);

    // диалоги
    std::vector<lf::Line> dlg;
    size_t dlgIdx = 0;
    float dlgT = 0;
    std::function<void()> dlgDone;
    void dialogue(const std::vector<lf::Line>& lines, std::function<void()> done = nullptr);
    bool drawDialogue();

    // деревня
    int selected = -1;
    int selectedOb = -1;  // выбранное препятствие
    void drawObstaclePanel();
    Placing placing;
    int mapTab = 0, shopTab = 0, codexPage = 0;
    bool resetArmed = false;
    void enterVillage();
    void villageFrame(float dt, const std::vector<input::Event>& world);
    void villageInput();
    void drawVillageWorld();
    void drawVillageHud();
    void drawSelectedPanel();
    void drawPlacingPanel();
    void drawShop();
    void drawArmy();
    void drawMap();
    void drawNight();
    void drawCodex();
    void drawMenu();
    void startPlacing(lf::BType t);
    void validatePlacing();
    void confirmPlacing();
    int buildingAt(float wx, float wy) const;
    bool modalFrame(const char* title, float w, float h, ui::Rect& body);

    // бой
    BattleState bt;
    void enterBattle(const lf::BattleOptions& o, BattleKind kind, const std::string& title, int missionIdx = -1,
                     const lf::replay::Record* resume = nullptr);
    bool act(lf::replay::Act kind, int arg, Vec2 w);  // действие игрока в бою (с записью)
    void saveBattle();
    bool resumeBattle();
    void battleFrame(float dt, const std::vector<input::Event>& world);
    void battleInput();
    void drawBattleWorld();
    void drawBattleHud();
    void finishBattle();
    void drawResult();
    bool tryBattleAction(Vec2 w);
};

std::string fmtNum(long long n);
std::string fmtTime(double seconds);

}  // namespace client
