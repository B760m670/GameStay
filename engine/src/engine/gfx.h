#pragma once
// Пакетный 2D-рендер: все квады кадра собираются на CPU, одна запись в вершинный буфер,
// затем по одному вызову отрисовки на смену текстуры/режима смешивания.
#include <cstdint>
#include <string>
#include <vector>

namespace gfx {

struct Texture {
    uint32_t img = 0, view = 0;  // sg_image / sg_view ids
    int w = 0, h = 0;
};

enum class Blend { Alpha, Add };

struct Vtx {
    float x, y, u, v;
    uint32_t color;
};

inline uint32_t rgba(int r, int g, int b, int a = 255) {
    return (uint32_t)(r & 255) | ((uint32_t)(g & 255) << 8) | ((uint32_t)(b & 255) << 16) | ((uint32_t)(a & 255) << 24);
}
inline uint32_t withAlpha(uint32_t c, float a) {
    int na = (int)(((c >> 24) & 255) * a);
    return (c & 0x00ffffffu) | ((uint32_t)(na < 0 ? 0 : na > 255 ? 255 : na) << 24);
}
uint32_t lerpColor(uint32_t a, uint32_t b, float t);

bool init();
void shutdown();

Texture* createTexture(int w, int h, const uint8_t* rgba, bool mipmaps = true);
Texture* loadTexture(const std::string& path, bool mipmaps = true);
Texture* white();

// Размер кадра в логических точках (для iPhone — points, а не пиксели).
void beginFrame(float width, float height);
float width();
float height();
void setBlend(Blend b);
// Масштаб вокруг точки для всего, что рисуется до pop (анимации UI). Вкладывается.
void pushScale(float cx, float cy, float s, float alpha = 1.f);
void popScale();
void quad(const Texture* t, const Vtx v[4]);
void rect(const Texture* t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color);
void fillRect(float x, float y, float w, float h, uint32_t color);
void endFrame(uint32_t clear);

// Для автотестов: сохранить последний кадр в PNG (только OpenGL).
bool screenshot(const std::string& path);

}  // namespace gfx
