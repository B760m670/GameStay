#pragma once
// Изометрическая камера (клетка 64×32 точки при zoom=1) и распознавание жестов.
#include <functional>
#include <map>
#include <vector>

#include "engine/input.h"

namespace client {

constexpr float kTW = 64.f, kTH = 32.f;

struct Vec2 { float x, y; };

struct Camera {
    float ox = 0, oy = 0, zoom = 1, w = 1, h = 1;
    float minZoom = 0.3f, maxZoom = 2.2f;

    Vec2 toScreen(float wx, float wy, float z = 0) const {
        return {ox + (wx - wy) * (kTW / 2) * zoom, oy + (wx + wy) * (kTH / 2) * zoom - z * zoom};
    }
    Vec2 toWorld(float sx, float sy) const {
        float a = (sx - ox) / ((kTW / 2) * zoom), b = (sy - oy) / ((kTH / 2) * zoom);
        return {(a + b) / 2, (b - a) / 2};
    }
    void centerOn(float wx, float wy);
    void focus(float wx, float wy, float radiusTiles, float maxZ = 1.2f);
    void zoomAt(float sx, float sy, float factor);
    void pan(float dx, float dy);
};

// Жесты: тап, перетаскивание (сцена может перехватить), панорама, щипок, колесо.
class Gestures {
public:
    std::function<bool(Vec2 world, Vec2 screen)> onDown;   // вернуть true — перехватить перетаскивание
    std::function<void(Vec2 world, Vec2 screen)> onDrag;
    std::function<void(bool moved, Vec2 world)> onDragEnd;
    std::function<void(Vec2 world, Vec2 screen)> onTap;

    void process(const std::vector<input::Event>& events, Camera& cam);
    void reset() { pointers_.clear(); drag_ = Drag{}; pinch_ = false; }

private:
    struct Drag { bool active = false, custom = false, moved = false; int id = -1; float sx = 0, sy = 0, x = 0, y = 0; };
    std::map<int, Vec2> pointers_;
    Drag drag_;
    bool pinch_ = false;
    float pinchDist_ = 0;
};

}  // namespace client
