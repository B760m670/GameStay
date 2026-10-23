#pragma once
// Немедленный UI: виджеты рисуются каждый кадр и сами проверяют нажатия.
// Касание, начатое на виджете, «съедается» UI и не двигает камеру.
#include <cstdint>
#include <string>
#include <vector>

#include "input.h"

namespace ui {

struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

enum class Style { Normal, Gold, Green, Red, Dark, Paper, Card };

// Принимает события кадра, возвращает те, что достались миру.
std::vector<input::Event> beginFrame(const std::vector<input::Event>& events);
void endFrame();

// Зарегистрировать область, перехватывающую касания (панели, затемнения модалок).
void block(const Rect& r);
void panel(const Rect& r, Style s = Style::Normal);
bool button(const Rect& r, const std::string& label, Style s = Style::Normal, bool enabled = true, float textSize = 0);
// Кнопка-иконка с подписью снизу.
bool iconButton(const Rect& r, const std::string& icon, const std::string& label, Style s = Style::Normal, bool highlight = false);
void label(const std::string& text, float x, float y, float size, uint32_t color, int align = 0);
void bar(const Rect& r, float frac, uint32_t color);
// Поле ввода: касание — фокус и экранная клавиатура. Возвращает true, когда нажат «Ввод».
bool textField(const Rect& r, std::string& text, const std::string& placeholder, int maxRunes = 120);
bool textFocused();
void icon(const std::string& name, float cx, float cy, float size, uint32_t color = 0xffffffffu);
// Круглая кнопка действия (как под выбранным зданием в CoC): иконка внутри, подпись снизу.
bool roundButton(float cx, float cy, float r, const std::string& icon, const std::string& label, Style s = Style::Normal, bool enabled = true);
// Плитка действия (панель выбранной постройки): иконка, подпись и необязательная вторая строка — цена с иконкой ресурса.
// dim — приглушить (действие сейчас недоступно), но касание всё равно сообщается, чтобы показать причину.
bool tile(const Rect& r, const std::string& icon, const std::string& label, Style s, bool dim = false, const std::string& sub = "",
          const std::string& subIcon = "", uint32_t subColor = 0xffffffffu);
// Красный кружок со счётчиком (непрочитанное, готовые награды).
void countBadge(float cx, float cy, int n);
// Замок поверх недоступной карточки: затемнение, значок и причина.
void lockOverlay(const Rect& r, const std::string& reason);
// Лента-заголовок.
void ribbon(const std::string& text, float cx, float y, float w);
// Время для анимаций (секунды).
void setTime(float t);
// Прямоугольник, по которому было касание-клик (без отрисовки).
bool tapped(const Rect& r);

}  // namespace ui
