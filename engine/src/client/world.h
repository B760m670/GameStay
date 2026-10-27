#pragma once
// Отрисовка мира: земля, постройки, юниты, снаряды, эффекты.
#include <string>
#include <vector>

#include "camera.h"
#include "game/core.h"

namespace atlas { struct Sprite; }

namespace client::world {

const atlas::Sprite* buildingSprite(lf::BType type, int level);
// Частокол по клеткам: links для клетки (x, y) — есть ли звенья к соседям по +X и +Y.
int wallLinks(const std::vector<uint8_t>& wallAt, int x, int y);

constexpr int kPad = 9;  // сколько клеток мира рисуем за краем игрового поля (лес, скалы)
void drawGround(const Camera& cam);
// Мягкая тень у основания постройки (подложек под зданиями нет).
void drawContactShadow(const Camera& cam, float x, float y, float size);
// Детали луга (трава, цветы, камешки) — под постройками.
void drawMeadow(const Camera& cam, float t);
// Блики на ручье и рябь на пруду.
void drawWaterFx(const Camera& cam, float t);
// Ручей вокруг поселения: середина русла (для обеих сторон) и расстояние до воды (< 0 — в воде).
float streamCenter(float t);
float waterDist(float x, float y);
struct Decor { const char* sprite; float x, y; int size; float depth; int fx = 0; };  // fx: 1 — костёр, 2 — на воде
// Лес и скалы за краем поля (сортируются вместе с постройками по глубине).
const std::vector<Decor>& decor();
void drawDecor(const Camera& cam, const Decor& d, float t);
// Освещение сцены: цвет, на который умножаются постройки, юниты и декор (день — белый, сумерки набега — темнее).
void setAmbient(uint32_t color);
uint32_t ambientTint(uint32_t base);

struct BuildingLook {
    float alpha = 1, dim = 1;      // dim < 1 — затемнение
    bool selected = false, dead = false, constructing = false;
    int ghost = 0;                 // 1 — можно поставить, -1 — нельзя
    float hitFlash = 0;
    float collapse = -1;           // 0..1 — идёт обрушение разрушенной постройки
    float angle = 0.785f;          // куда смотрит поворотная часть (мировой угол), по умолчанию — к зрителю
    float pop = -1;                // 0..1 — «пружина» (выбор, установка, завершение стройки)
    int links = 0;                 // частокол: 1 — сосед по +X, 2 — сосед по +Y (рисуются звенья к ним)
};
void drawBuilding(const Camera& cam, lf::BType type, float x, float y, int level, const BuildingLook& look, float t);
void drawFootprint(const Camera& cam, float x, float y, float size, uint32_t color);
void drawFootprintOutline(const Camera& cam, float x, float y, float size, uint32_t color, float width = 2.f);
// Сетка клеток вокруг точки (при перемещении постройки), гаснет к краю радиуса.
void drawGridAround(const Camera& cam, float cx, float cy, float r);
void drawBarAt(const Camera& cam, float wx, float wy, float z, float frac, uint32_t color, float width = 40);

// deadT >= 0 — сколько секунд назад юнит погиб (анимация смерти, затем тело тает).
void drawTroop(const Camera& cam, const lf::BTroop& tr, float t, float deadT = -1);
// Персонаж вне боя (жители, рабочие, войска у лагеря): unit — имя набора спрайтов, angle — мировой угол.
void drawUnit(const Camera& cam, const std::string& unit, const std::string& anim, float angle, int frame, float x, float y, float alpha = 1.f);

// Частицы: искры, дым, блики, светлячки, пыль, обломки (chunk — твёрдый кусок, падает под гравитацией).
struct Particle { float x, y, z, vx, vy, vz, life, max, size; uint32_t color; bool additive; bool chunk = false; };
void updateParticles(std::vector<Particle>& ps, float dt);
void drawParticle(const Camera& cam, const Particle& p);
// Мягкое аддитивное свечение в экранных координатах.
void glowSpot(float x, float y, float r, uint32_t color);
void drawProjectile(const Camera& cam, const lf::Projectile& p);
void drawEffect(const Camera& cam, const lf::Effect& e, float t);
// Зона приёма (Лекарь, Боевой рог) на земле: кольцо и частицы.
void drawSpellZone(const Camera& cam, const lf::SpellZone& z, float battleTime, float t);

// Иконка-миниатюра постройки/юнита для UI.
void drawBuildingThumb(lf::BType type, float cx, float cy, float maxW, float maxH, int level = 1);
void drawTroopThumb(lf::TroopType type, float cx, float cy, float size);

}  // namespace client::world
