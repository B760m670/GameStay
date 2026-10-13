#include "camera.h"

#include <algorithm>
#include <cmath>

#include "game/core.h"

namespace client {

void Camera::centerOn(float wx, float wy) {
    Vec2 s = toScreen(wx, wy);
    ox += w / 2 - s.x;
    oy += h / 2 - s.y;
}

void Camera::focus(float wx, float wy, float R, float maxZ) {
    float zw = (w * 0.92f) / (1.41f * R * kTW), zh = (h * 0.8f) / (0.71f * R * kTW);
    zoom = std::clamp(std::min(zw, zh), minZoom, maxZ);
    ox = oy = 0;
    centerOn(wx, wy);
}

void Camera::zoomAt(float sx, float sy, float f) {
    Vec2 before = toWorld(sx, sy);
    zoom = std::clamp(zoom * f, minZoom, maxZoom);
    Vec2 after = toScreen(before.x, before.y);
    ox += sx - after.x;
    oy += sy - after.y;
}

void Camera::pan(float dx, float dy) {
    ox += dx;
    oy += dy;
    Vec2 c = toWorld(w / 2, h / 2);
    float cx = std::clamp(c.x, -4.f, lf::kGrid + 4.f), cy = std::clamp(c.y, -4.f, lf::kGrid + 4.f);
    if (cx != c.x || cy != c.y) centerOn(cx, cy);
}

void Gestures::process(const std::vector<input::Event>& events, Camera& cam) {
    for (const auto& e : events) {
        Vec2 p{e.x, e.y};
        switch (e.type) {
            case input::Type::Scroll:
                cam.zoomAt(e.x, e.y, std::exp(e.scroll * 0.12f));
                break;
            case input::Type::Down: {
                pointers_[e.id] = p;
                if (pointers_.size() == 2) {
                    auto it = pointers_.begin();
                    Vec2 a = it->second, b = (++it)->second;
                    pinch_ = true;
                    pinchDist_ = std::hypot(a.x - b.x, a.y - b.y);
                    if (drag_.active && drag_.custom && onDragEnd) onDragEnd(false, cam.toWorld(p.x, p.y));
                    drag_ = Drag{};
                    break;
                }
                drag_ = Drag{true, false, false, e.id, e.x, e.y, e.x, e.y};
                if (onDown) drag_.custom = onDown(cam.toWorld(e.x, e.y), p);
                break;
            }
            case input::Type::Move: {
                if (!pointers_.count(e.id)) break;
                pointers_[e.id] = p;
                if (pinch_ && pointers_.size() >= 2) {
                    auto it = pointers_.begin();
                    Vec2 a = it->second, b = (++it)->second;
                    float d = std::hypot(a.x - b.x, a.y - b.y);
                    if (pinchDist_ > 1) cam.zoomAt((a.x + b.x) / 2, (a.y + b.y) / 2, d / pinchDist_);
                    pinchDist_ = d;
                    break;
                }
                if (!drag_.active || drag_.id != e.id) break;
                float dx = e.x - drag_.x, dy = e.y - drag_.y;
                drag_.x = e.x;
                drag_.y = e.y;
                if (std::hypot(e.x - drag_.sx, e.y - drag_.sy) > 9) drag_.moved = true;
                if (drag_.custom) { if (onDrag) onDrag(cam.toWorld(e.x, e.y), p); }
                else if (drag_.moved) cam.pan(dx, dy);
                break;
            }
            case input::Type::Up:
            case input::Type::Cancel: {
                pointers_.erase(e.id);
                if (pointers_.size() < 2) pinch_ = false;
                if (!drag_.active || drag_.id != e.id) break;
                if (drag_.custom && onDragEnd) onDragEnd(drag_.moved, cam.toWorld(e.x, e.y));
                if (!drag_.moved && e.type == input::Type::Up && onTap) onTap(cam.toWorld(e.x, e.y), p);
                drag_ = Drag{};
                break;
            }
        }
    }
}

}  // namespace client
