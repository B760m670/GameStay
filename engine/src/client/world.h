#pragma once
// Отрисовка мира: земля, туман Мглы и свет, постройки, юниты, снаряды, эффекты.
#include <string>
#include <vector>

#include "camera.h"
#include "game/core.h"

namespace atlas { struct Sprite; }

namespace client::world {

const atlas::Sprite* buildingSprite(lf::BType type, int level);

constexpr int kPad = 9;  // сколько клеток мира рисуем за краем игрового поля (лес, скалы)
void drawGround(const Camera& cam);
// Мягкая тень у основания постройки (подложек под зданиями нет).
void drawContactShadow(const Camera& cam, float x, float y, float size);
// Детали луга (трава, цветы, камешки) — под постройками.
void drawMeadow(const Camera& cam, float t);
// Блики на ручье и рябь на пруду (поверх тумана — лунный свет на воде).
void drawWaterFx(const Camera& cam, float t);
// Ручей вокруг поселения: середина русла (для обеих сторон) и расстояние до воды (< 0 — в воде).
float streamCenter(float t);
float waterDist(float x, float y);
struct Decor { const char* sprite; float x, y; int size; float depth; int fx = 0; };  // fx: 1 — костёр, 2 — на воде
// Лес и скалы за краем поля (сортируются вместе с постройками по глубине).
const std::vector<Decor>& decor();
void drawDecor(const Camera& cam, const Decor& d, float t);
// Освещённость точки мира по последнему drawFog (0 — тьма, 1 — свет) и цвет base, затенённый по ней.
float lightAt(float x, float y);
uint32_t lightTint(float x, float y, uint32_t base);
// Туман: темнота везде, кроме кругов света. Плюс тёплое свечение источников.
void drawFog(const Camera& cam, const std::vector<lf::LightSource>& src, const std::vector<lf::Eclipse>& ecl, float t,
             float darkness = 0.74f, bool forbidTint = false);

struct BuildingLook {
    float alpha = 1, dim = 1;      // dim < 1 — затемнение (обесточена)
    bool selected = false, dead = false, unpowered = false, constructing = false;
    int ghost = 0;                 // 1 — можно поставить, -1 — нельзя
    float hitFlash = 0;
    float collapse = -1;           // 0..1 — идёт обрушение разрушенной постройки
    float angle = 0.785f;          // куда смотрит поворотная часть (мировой угол), по умолчанию — к зрителю
    float stunned = 0;             // > 0 — ослеплена Вспышкой
    float pop = -1;                // 0..1 — «пружина» (выбор, установка, завершение стройки)
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
void drawBeam(const Camera& cam, const lf::BBuilding& b, const lf::BTroop& tr);
void drawEffect(const Camera& cam, const lf::Effect& e, float t);
// Зона заклинания на земле: кольцо, свечение, искры.
void drawSpellZone(const Camera& cam, const lf::SpellZone& z, float battleTime, float t);

// Иконка-миниатюра постройки/юнита для UI.
void drawBuildingThumb(lf::BType type, float cx, float cy, float maxW, float maxH, int level = 1);
void drawTroopThumb(lf::TroopType type, float cx, float cy, float size);

}  // namespace client::world
