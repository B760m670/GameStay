#pragma once
// Растровый шрифт из TTF (stb_truetype): латиница, кириллица, типографские знаки.
#include <string>
#include <vector>

#include "gfx.h"

namespace font {

enum Align { Left, Center, Right };

bool load(const std::string& ttfPath, float bakePx = 56.f);
// Рисует строку UTF-8. size — высота кегля в логических точках. y — базовая линия сверху (верх строки).
float draw(const std::string& text, float x, float y, float size, uint32_t color, Align align = Left, bool outline = true);
float measure(const std::string& text, float size);
// Перенос по ширине: возвращает строки.
std::vector<std::string> wrap(const std::string& text, float size, float maxWidth);
float lineHeight(float size);

}  // namespace font
