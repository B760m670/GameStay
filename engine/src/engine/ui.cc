#include "ui.h"

#include <algorithm>
#include <cmath>

#include "atlas.h"
#include "audio.h"
#include "font.h"
#include "gfx.h"

namespace ui {
namespace {
std::vector<Rect> prevBlocks, curBlocks;
int activeId = -1;              // палец, захваченный UI
float pressX = 0, pressY = 0, curX = 0, curY = 0;
bool released = false;          // палец отпущен в этом кадре
float relX = 0, relY = 0;
float uiTime = 0;
std::vector<std::pair<Rect, float>> bounces;  // недавно нажатые кнопки: время нажатия

float bounceScale(const Rect& r, bool pressed) {
    if (pressed) return 0.92f;
    for (const auto& [br, t0] : bounces) {
        if (std::fabs(br.x - r.x) < 1 && std::fabs(br.y - r.y) < 1) {
            float k = uiTime - t0;
            if (k < 0.35f) return 1.f + std::sin(k * 18.f) * 0.08f * (1 - k / 0.35f);
        }
    }
    return 1.f;
}
void addBounce(const Rect& r) {
    bounces.push_back({r, uiTime});
    if (bounces.size() > 8) bounces.erase(bounces.begin());
}

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
        case Style::Paper: return "ui/paper";
        case Style::Card: return "ui/card";
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

void setTime(float t) { uiTime = t; }

void panel(const Rect& r, Style s) {
    block(r);
    if (s == Style::Paper) atlas::draw9(atlas::get("ui/paper"), r.x, r.y, r.w, r.h, 66, 30);
    else if (s == Style::Card) atlas::draw9(atlas::get("ui/card"), r.x, r.y, r.w, r.h, 40, 16);
    else atlas::draw9(atlas::get(s == Style::Dark ? "ui/panel_dark" : "ui/panel"), r.x, r.y, r.w, r.h, 70, 24);
}

void ribbon(const std::string& text, float cx, float y, float w) {
    // лента из ткани: концы с хвостами не растягиваются, середина тянется (3-slice)
    const atlas::Sprite* s = atlas::get("ui/ribbon");
    if (s) {
        float h = 50, k = h / s->h, cap = 120 * k, cu = 120 / s->w * (s->u1 - s->u0);
        float x0 = cx - w / 2 - cap * 0.55f, x1 = cx + w / 2 + cap * 0.55f, top = y - 5;
        gfx::rect(s->tex, x0, top, cap, h, s->u0, s->v0, s->u0 + cu, s->v1, 0xffffffffu);
        gfx::rect(s->tex, x0 + cap, top, x1 - x0 - 2 * cap, h, s->u0 + cu, s->v0, s->u1 - cu, s->v1, 0xffffffffu);
        gfx::rect(s->tex, x1 - cap, top, cap, h, s->u1 - cu, s->v0, s->u1, s->v1, 0xffffffffu);
    }
    font::draw(text, cx, y + 7, 21, 0xffffffffu, font::Center);
}

bool roundButton(float cx, float cy, float r, const std::string& ic, const std::string& text, Style s, bool enabled) {
    Rect rect{cx - r, cy - r, r * 2, r * 2 + 18};
    bool clicked = tapped(rect) && enabled;
    bool pressed = activeId >= 0 && rect.contains(pressX, pressY) && rect.contains(curX, curY);
    if (clicked) { audio::play("click", 0.6f); addBounce(rect); }
    float k = bounceScale(rect, pressed);
    gfx::pushScale(cx, cy, k);
    const char* spr = s == Style::Gold ? "ui/round_gold" : s == Style::Green ? "ui/round_green" : s == Style::Red ? "ui/round_red" : "ui/round";
    atlas::drawRect(atlas::get(spr), cx - r, cy - r, r * 2, r * 2, enabled ? 0xffffffffu : gfx::rgba(150, 150, 160, 220));
    icon(ic, cx, cy - r * 0.06f, r * 1.25f, enabled ? 0xffffffffu : gfx::rgba(180, 180, 180, 220));
    font::draw(text, cx, cy + r - 2, 14, 0xffffffffu, font::Center);
    gfx::popScale();
    return clicked;
}

bool button(const Rect& r, const std::string& text, Style s, bool enabled, float textSize) {
    bool clicked = tapped(r) && enabled;
    bool pressed = activeId >= 0 && r.contains(pressX, pressY) && r.contains(curX, curY);
    if (clicked) { audio::play("click", 0.6f); addBounce(r); }
    float k = bounceScale(r, pressed && enabled);
    gfx::pushScale(r.x + r.w / 2, r.y + r.h / 2, k);
    uint32_t tint = enabled ? 0xffffffffu : gfx::rgba(150, 150, 160, 220);
    atlas::draw9(atlas::get(styleSprite(s)), r.x, r.y, r.w, r.h, 56, 22, tint);
    float ts = textSize > 0 ? textSize : std::min(20.f, r.h * 0.42f);
    font::draw(text, r.x + r.w / 2, r.y + (r.h - 6 - font::lineHeight(ts)) / 2 + 1, ts, enabled ? 0xffffffffu : gfx::rgba(210, 210, 210), font::Center);
    gfx::popScale();
    return clicked;
}

bool iconButton(const Rect& r, const std::string& ic, const std::string& text, Style s, bool highlight) {
    bool clicked = tapped(r);
    bool pressed = activeId >= 0 && r.contains(pressX, pressY) && r.contains(curX, curY);
    if (clicked) { audio::play("click", 0.6f); addBounce(r); }
    float k = bounceScale(r, pressed);
    gfx::pushScale(r.x + r.w / 2, r.y + r.h / 2, k);
    if (highlight) atlas::draw9(atlas::get("ui/glow_frame"), r.x - 6, r.y - 6, r.w + 12, r.h + 12, 30, 18, gfx::rgba(255, 220, 120, 230));
    atlas::draw9(atlas::get(styleSprite(s)), r.x, r.y, r.w, r.h, 56, 22);
    icon(ic, r.x + r.w / 2, r.y + r.h * 0.38f, r.h * 0.62f);
    font::draw(text, r.x + r.w / 2, r.y + r.h * 0.64f, std::min(14.f, r.h * 0.2f), 0xffffffffu, font::Center);
    gfx::popScale();
    return clicked;
}

void label(const std::string& text, float x, float y, float size, uint32_t color, int align) {
    font::draw(text, x, y, size, color, (font::Align)align);
}

void bar(const Rect& r, float frac, uint32_t color) {
    frac = std::clamp(frac, 0.f, 1.f);
    atlas::draw9(atlas::get("ui/bar_bg"), r.x - 2, r.y - 2, r.w + 4, r.h + 4, 30, (r.h + 4) / 2);
    if (frac > 0.01f) atlas::draw9(atlas::get("ui/bar_fill"), r.x + 1, r.y + 1, std::max(r.h, (r.w - 2) * frac), r.h - 2, 26, (r.h - 2) / 2, color);
}

void icon(const std::string& name, float cx, float cy, float size, uint32_t color) {
    const atlas::Sprite* s = atlas::get(name);
    if (!s) return;
    float k = size / std::max(s->w, s->h);
    atlas::drawRect(s, cx - s->w * k / 2, cy - s->h * k / 2, s->w * k, s->h * k, color);
}

}  // namespace ui
