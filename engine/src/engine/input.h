#pragma once
// Ввод: касания и мышь приводятся к единому потоку событий в логических точках.
#include <string>
#include <vector>

struct sapp_event;

namespace input {

enum class Type { Down, Move, Up, Cancel, Scroll };
struct Event {
    Type type;
    int id;          // идентификатор пальца (мышь = 0)
    float x, y;      // логические точки
    float scroll = 0;
};

void handle(const sapp_event* e);
std::vector<Event> drain();
// Набранный текст с прошлого кадра (UTF-8), стёртые символы и нажатие «Ввод».
struct Text { std::string chars; int backspaces = 0; bool enter = false; };
Text takeText();
// Для автотестов: подмешать событие.
void inject(const Event& e);
// Клавиша зажата (код SAPP_KEYCODE_*); setKey — для автотестов.
bool key(int code);
void setKey(int code, bool down);

}  // namespace input
