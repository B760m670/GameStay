#pragma once
// Атласы спрайтов, собранные art/pack_atlas.py: страницы PNG + индекс atlas.txt.
#include <string>
#include <unordered_map>

#include "gfx.h"

namespace atlas {

struct Sprite {
    const gfx::Texture* tex = nullptr;
    float u0, v0, u1, v1;
    float w, h;    // размер в пикселях спрайта (Retina, 128 px на клетку)
    float px, py;  // опорная точка внутри спрайта (центр клетки на земле)
};

bool load(const std::string& dir);
const Sprite* get(const std::string& name);
// Нарисовать спрайт так, чтобы опорная точка оказалась в (x, y). scale: 0.5 = 1 пиксель спрайта на 0.5 точки.
void draw(const Sprite* s, float x, float y, float scale, uint32_t color = 0xffffffffu, bool flipX = false);
// Растянуть спрайт в прямоугольник (для UI).
void drawRect(const Sprite* s, float x, float y, float w, float h, uint32_t color = 0xffffffffu);
// 9-slice: border — ширина нерастягиваемого края в пикселях спрайта, drawBorder — в точках.
void draw9(const Sprite* s, float x, float y, float w, float h, float border, float drawBorder, uint32_t color = 0xffffffffu);

}  // namespace atlas
