#include "input.h"

#include "sokol_app.h"

namespace input {
namespace {
std::vector<Event> queue;
bool mouseDown = false;
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

}  // namespace input
