// Запись боя для продолжения после закрытия приложения: исходные условия + действия игрока по шагам.
// Симуляция детерминирована, поэтому повтор действий восстанавливает бой в точности.
#include <sstream>

#include "core.h"

namespace lf::replay {

bool apply(Battle& B, const Action& a) {
    switch (a.kind) {
        case Act::Deploy: return B.deploy((TroopType)a.arg, a.x, a.y);
        case Act::Reserved: return false;
        case Act::Spell: return B.castSpell((Spell)a.arg, a.x, a.y);
        case Act::Hero: return B.deployHero(a.x, a.y);
        case Act::HeroAbility: return B.heroAbility();
    }
    return false;
}

void fastForward(Battle& B, const Record& r, float dt) {
    size_t ai = 0;
    for (int s = 0; s <= r.steps; s++) {
        while (ai < r.actions.size() && r.actions[ai].step == s) apply(B, r.actions[ai++]);
        if (s < r.steps) B.step(dt);
    }
    B.events.clear();
}

std::string save(const Record& r) {
    std::ostringstream o;
    o.precision(9);
    const auto& op = r.opts;
    o << "replay 1\n";
    o << "opts " << (int)op.mode << " " << op.seed << " " << op.duration << " " << op.raidMult << " " << 0 << " " << op.heroLevel << "\n";
    for (const auto& it : op.layout)
        o << "b " << (int)it.type << " " << it.x << " " << it.y << " " << it.level << " " << it.id << " " << it.lootOre << " " << it.lootAether << "\n";
    for (auto [t, n] : op.army) o << "army " << (int)t << " " << n << "\n";
    for (auto [t, n] : op.troopLevels) o << "lvl " << (int)t << " " << n << "\n";
    for (auto [s, n] : op.spells) o << "spell " << (int)s << " " << n << "\n";
    for (const auto& w : op.waves) {
        o << "wave " << w.t << " " << (w.done ? 1 : 0);
        for (auto [t, n] : w.units) o << " " << (int)t << " " << n;
        o << "\n";
    }
    o << "steps " << r.steps << "\n";
    for (const auto& a : r.actions) o << "act " << a.step << " " << (int)a.kind << " " << a.arg << " " << a.x << " " << a.y << "\n";
    return o.str();
}

bool load(const std::string& s, Record& r) {
    std::istringstream in(s);
    std::string line;
    r = Record{};
    bool ok = false;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string k;
        l >> k;
        if (k == "replay") ok = true;
        else if (k == "opts") {
            int mode, reserved;  // reserved — бывшие заряды «Затмения», формат записи сохранён
            l >> mode >> r.opts.seed >> r.opts.duration >> r.opts.raidMult >> reserved >> r.opts.heroLevel;
            r.opts.mode = (BattleMode)mode;
        } else if (k == "b") {
            LayoutItem it;
            int t;
            l >> t >> it.x >> it.y >> it.level >> it.id >> it.lootOre >> it.lootAether;
            if (t < 0 || t >= kBTypes) return false;
            it.type = (BType)t;
            r.opts.layout.push_back(it);
        } else if (k == "army" || k == "lvl" || k == "spell") {
            int t, n;
            l >> t >> n;
            if (k == "spell") r.opts.spells[(Spell)t] = n;
            else if (t < 0 || t >= kTroopTypes) return false;
            else (k == "army" ? r.opts.army : r.opts.troopLevels)[(TroopType)t] = n;
        } else if (k == "wave") {
            Wave w;
            int done, t, n;
            l >> w.t >> done;
            w.done = done != 0;
            while (l >> t >> n) w.units[(TroopType)t] = n;
            r.opts.waves.push_back(w);
        } else if (k == "steps") l >> r.steps;
        else if (k == "act") {
            Action a;
            int kind;
            l >> a.step >> kind >> a.arg >> a.x >> a.y;
            a.kind = (Act)kind;
            r.actions.push_back(a);
        }
    }
    return ok && !r.opts.layout.empty();
}

}  // namespace lf::replay
