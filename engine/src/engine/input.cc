#include "input.h"

#include "sokol_app.h"

namespace input {
namespace {
std::vector<Event> queue;
bool mouseDown = false;
Text text;
bool keys[512];

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}
}

void handle(const sapp_event* e) {
    float k = 1.f / sapp_dpi_scale();
    switch (e->type) {
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
        case SAPP_EVENTTYPE_TOUCHES_MOVED:
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
        case SAPP_EVENTTYPE_TOUCHES_CANCELLED: {
            Type t = e->type == SAPP_EVENTTYPE_TOUCHES_BEGAN ? Type::Down
                   : e->type == SAPP_EVENTTYPE_TOUCHES_MOVED ? Type::Move
                   : e->type == SAPP_EVENTTYPE_TOUCHES_ENDED ? Type::Up : Type::Cancel;
            for (int i = 0; i < e->num_touches; i++) {
                const sapp_touchpoint& tp = e->touches[i];
                if (t != Type::Move && !tp.changed) continue;
                queue.push_back({t, (int)(tp.identifier & 0x7fffffff) + 1, tp.pos_x * k, tp.pos_y * k});
            }
            break;
        }
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) { mouseDown = true; queue.push_back({Type::Down, 0, e->mouse_x * k, e->mouse_y * k}); }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) { mouseDown = false; queue.push_back({Type::Up, 0, e->mouse_x * k, e->mouse_y * k}); }
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            queue.push_back({Type::Move, 0, e->mouse_x * k, e->mouse_y * k});
            break;
        case SAPP_EVENTTYPE_CHAR:
            if (e->char_code >= 32 && e->char_code != 127) appendUtf8(text.chars, e->char_code);
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (e->key_code >= 0 && e->key_code < 512) keys[e->key_code] = true;
            if (e->key_code == SAPP_KEYCODE_BACKSPACE) text.backspaces++;
            else if (e->key_code == SAPP_KEYCODE_ENTER || e->key_code == SAPP_KEYCODE_KP_ENTER) text.enter = true;
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            if (e->key_code >= 0 && e->key_code < 512) keys[e->key_code] = false;
            break;
        case SAPP_EVENTTYPE_UNFOCUSED:
            for (bool& k : keys) k = false;
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            queue.push_back({Type::Scroll, 0, e->mouse_x * k, e->mouse_y * k, e->scroll_y});
            break;
        default: break;
    }
}

std::vector<Event> drain() {
    std::vector<Event> out;
    out.swap(queue);
    return out;
}

void inject(const Event& e) { queue.push_back(e); }

bool key(int code) { return code >= 0 && code < 512 && keys[code]; }
void setKey(int code, bool down) { if (code >= 0 && code < 512) keys[code] = down; }

Text takeText() {
    Text t = std::move(text);
    text = Text{};
    return t;
}

}  // namespace input
