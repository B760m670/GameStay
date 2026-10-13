#include "ui.h"

#include <algorithm>
#include <cmath>

#include "atlas.h"
#include "font.h"
#include "gfx.h"

namespace ui {
namespace {
std::vector<Rect> prevBlocks, curBlocks;
int activeId = -1;              // палец, захваченный UI
float pressX = 0, pressY = 0, curX = 0, curY = 0;
bool released = false;          // палец отпущен в этом кадре
float relX = 0, relY = 0;

bool hitPrev(float x, float y) {
    for (const auto& r : prevBlocks) if (r.contains(x, y)) return true;
    return false;
}

const char* styleSprite(Style s) {
    switch (s) {
        case Style::Gold: return "ui/btn_gold";
        case Style::Green: return "ui/btn_green";
        case Style::Red: return "ui/btn_red";
        case Style::Dark: return "ui/btn_dark";
        default: return "ui/btn";
    }
}
}  // namespace

std::vector<input::Event> beginFrame(const std::vector<input::Event>& events) {
    released = false;
    std::vector<input::Event> world;
    for (const auto& e : events) {
        if (e.type == input::Type::Down && activeId < 0 && hitPrev(e.x, e.y)) {
            activeId = e.id;
            pressX = curX = e.x;
            pressY = curY = e.y;
            continue;
        }
        if (e.id == activeId) {
            if (e.type == input::Type::Move) { curX = e.x; curY = e.y; }
            if (e.type == input::Type::Up) { released = true; relX = e.x; relY = e.y; activeId = -1; }
            if (e.type == input::Type::Cancel) activeId = -1;
            continue;
        }
        if (e.type == input::Type::Scroll && hitPrev(e.x, e.y)) continue;
        world.push_back(e);
    }
    curBlocks.clear();
    return world;
}

void endFrame() { prevBlocks.swap(curBlocks); }

void block(const Rect& r) { curBlocks.push_back(r); }

bool tapped(const Rect& r) {
    block(r);
    return released && r.contains(relX, relY) && r.contains(pressX, pressY);
}

void panel(const Rect& r, Style s) {
    block(r);
    atlas::draw9(atlas::get(s == Style::Dark ? "ui/panel_dark" : "ui/panel"), r.x, r.y, r.w, r.h, 28, 14);
}

bool button(const Rect& r, const std::string& text, Style s, bool enabled, float textSize) {
    bool clicked = tapped(r) && enabled;
    bool pressed = activeId >= 0 && r.contains(pressX, pressY) && r.contains(curX, curY);
    float dy = pressed ? 2.f : 0.f;
    uint32_t tint = enabled ? 0xffffffffu : gfx::rgba(150, 150, 160, 220);
    atlas::draw9(atlas::get(styleSprite(s)), r.x, r.y + dy, r.w, r.h, 26, 13, tint);
    float ts = textSize > 0 ? textSize : std::min(20.f, r.h * 0.42f);
    font::draw(text, r.x + r.w / 2, r.y + dy + (r.h - font::lineHeight(ts)) / 2 + 1, ts, enabled ? 0xffffffffu : gfx::rgba(210, 210, 210), font::Center);
    return clicked;
}

bool iconButton(const Rect& r, const std::string& ic, const std::string& text, Style s, bool highlight) {
    bool clicked = tapped(r);
    bool pressed = activeId >= 0 && r.contains(pressX, pressY) && r.contains(curX, curY);
    float dy = pressed ? 2.f : 0.f;
    atlas::draw9(atlas::get(styleSprite(s)), r.x, r.y + dy, r.w, r.h, 26, 13);
    if (highlight) atlas::draw9(atlas::get("ui/glow_frame"), r.x - 6, r.y - 6 + dy, r.w + 12, r.h + 12, 30, 18, gfx::rgba(255, 220, 120, 230));
    icon(ic, r.x + r.w / 2, r.y + dy + r.h * 0.4f, r.h * 0.5f);
    font::draw(text, r.x + r.w / 2, r.y + dy + r.h * 0.68f, std::min(14.f, r.h * 0.2f), 0xffffffffu, font::Center);
    return clicked;
}

void label(const std::string& text, float x, float y, float size, uint32_t color, int align) {
    font::draw(text, x, y, size, color, (font::Align)align);
}

void bar(const Rect& r, float frac, uint32_t color) {
    frac = std::clamp(frac, 0.f, 1.f);
    atlas::draw9(atlas::get("ui/bar_bg"), r.x, r.y, r.w, r.h, 12, r.h / 2);
    if (frac > 0.01f) atlas::draw9(atlas::get("ui/bar_fill"), r.x + 2, r.y + 2, std::max(r.h, (r.w - 4) * frac), r.h - 4, 10, (r.h - 4) / 2, color);
}

void icon(const std::string& name, float cx, float cy, float size, uint32_t color) {
    const atlas::Sprite* s = atlas::get(name);
    if (!s) return;
    float k = size / std::max(s->w, s->h);
    atlas::drawRect(s, cx - s->w * k / 2, cy - s->h * k / 2, s->w * k, s->h * k, color);
}

}  // namespace ui
