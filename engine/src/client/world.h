#pragma once
// Отрисовка мира: земля, туман Мглы и свет, постройки, юниты, снаряды, эффекты.
#include <string>
#include <vector>

#include "camera.h"
#include "game/core.h"

namespace atlas { struct Sprite; }

namespace client::world {

const atlas::Sprite* buildingSprite(lf::BType type, int level);

void drawGround(const Camera& cam);
// Туман: темнота везде, кроме кругов света. Плюс тёплое свечение источников.
void drawFog(const Camera& cam, const std::vector<lf::LightSource>& src, const std::vector<lf::Eclipse>& ecl, float t,
             float darkness = 0.74f, bool forbidTint = false);

struct BuildingLook {
    float alpha = 1, dim = 1;      // dim < 1 — затемнение (обесточена)
    bool selected = false, dead = false, unpowered = false, constructing = false;
    int ghost = 0;                 // 1 — можно поставить, -1 — нельзя
    float hitFlash = 0;
};
void drawBuilding(const Camera& cam, lf::BType type, float x, float y, int level, const BuildingLook& look, float t);
void drawFootprint(const Camera& cam, float x, float y, float size, uint32_t color);
void drawBarAt(const Camera& cam, float wx, float wy, float z, float frac, uint32_t color, float width = 40);

void drawTroop(const Camera& cam, const lf::BTroop& tr, float t);
void drawProjectile(const Camera& cam, const lf::Projectile& p);
void drawBeam(const Camera& cam, const lf::BBuilding& b, const lf::BTroop& tr);
void drawEffect(const Camera& cam, const lf::Effect& e, float t);

// Иконка-миниатюра постройки/юнита для UI.
void drawBuildingThumb(lf::BType type, float cx, float cy, float maxW, float maxH, int level = 1);
void drawTroopThumb(lf::TroopType type, float cx, float cy, float size);

}  // namespace client::world
