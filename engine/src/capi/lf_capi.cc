// C-интерфейс ядра для сервера: те же правила, что на телефоне.
#include "lf_capi.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "game/core.h"

using namespace lf;

namespace {
char* dup(const std::string& s) {
    char* p = (char*)std::malloc(s.size() + 1);
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}
void setErr(char** e, const std::string& s) { if (e) *e = dup(s); }

bool loadSave(const char* text, VillageState& S) { return text && village::load(text, S); }

bool sameLayout(const std::vector<LayoutItem>& a, const std::vector<LayoutItem>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        const auto &p = a[i], &q = b[i];
        if (p.type != q.type || p.x != q.x || p.y != q.y || p.level != q.level) return false;
        if (std::fabs(p.lootOre - q.lootOre) > 1 || std::fabs(p.lootAether - q.lootAether) > 1) return false;
    }
    return true;
}
}  // namespace

extern "C" {

char* lf_new_village(long long now) { return dup(village::save(village::create(now))); }

char* lf_tick_save(const char* save, long long now) {
    VillageState S;
    if (!loadSave(save, S)) return nullptr;
    village::tick(S, now);
    return dup(village::save(S));
}

char* lf_summary(const char* save) {
    VillageState S;
    if (!loadSave(save, S)) return nullptr;
    std::ostringstream o;
    o << "level " << S.level << "\nrating " << S.rating << "\nlantern " << (village::hqRuined(S) ? 0 : village::lanternLevel(S)) << "\nore " << S.ore << "\naether " << S.aether
      << "\nshards " << S.shards << "\n";
    return dup(o.str());
}

char* lf_snapshot(const char* save, double lootFrac) {
    VillageState S;
    if (!loadSave(save, S)) return nullptr;
    replay::Record r;
    r.opts.layout = village::layout(S, (float)lootFrac);
    std::string full = replay::save(r), out;
    std::istringstream in(full);
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("b ", 0) == 0) out += line + "\n";
    return dup(out);
}

char* lf_verify_attack(const char* attackerSave, const char* snapshot, const char* replayText, int expectedSeed, long long now,
                       char** attackerOut, char** error) {
    VillageState S;
    if (!loadSave(attackerSave, S)) { setErr(error, "bad attacker save"); return nullptr; }
    village::tick(S, now);
    replay::Record snap, rec;
    if (!snapshot || !replay::load(std::string("replay 1\n") + snapshot, snap)) { setErr(error, "bad snapshot"); return nullptr; }
    if (!replayText || !replay::load(replayText, rec)) { setErr(error, "bad replay"); return nullptr; }
    const auto& o = rec.opts;
    if (o.mode != BattleMode::Attack) { setErr(error, "not an attack"); return nullptr; }
    if (o.seed != expectedSeed) { setErr(error, "seed mismatch"); return nullptr; }
    if (std::fabs(o.duration - kBattleTime) > 0.01f) { setErr(error, "duration mismatch"); return nullptr; }
    if (!sameLayout(o.layout, snap.opts.layout)) { setErr(error, "layout mismatch"); return nullptr; }
    for (auto [t, n] : o.army)
        if ((int)t >= kPlayerTroops || n < 0 || n > S.army[(int)t]) { setErr(error, "army exceeds what the player has"); return nullptr; }
    for (auto [t, lv] : o.troopLevels)
        if ((int)t >= kPlayerTroops || lv != S.troopLevel[(int)t]) { setErr(error, "troop level mismatch"); return nullptr; }
    auto allowed = village::spellCharges(S.level);
    for (auto [sp, n] : o.spells)
        if (n > (allowed.count(sp) ? allowed[sp] : 0)) { setErr(error, "spell charges exceed level"); return nullptr; }
    if (o.heroLevel != 0 && (!village::heroUnlocked(S) || o.heroLevel != S.heroLevel)) { setErr(error, "hero mismatch"); return nullptr; }
    const float dt = 1.f / 30.f;
    if (rec.steps < 0 || rec.steps > (int)(kBattleTime / dt) + 2) { setErr(error, "too many steps"); return nullptr; }
    for (size_t i = 1; i < rec.actions.size(); i++)
        if (rec.actions[i].step < rec.actions[i - 1].step) { setErr(error, "actions out of order"); return nullptr; }
    Battle B(o);
    replay::fastForward(B, rec, dt);
    if (!B.ended) B.finish();
    auto out = village::applyRaid(S, B.result);
    if (attackerOut) *attackerOut = dup(village::save(S));
    std::ostringstream res;
    res << "stars " << B.result.stars << "\npct " << B.result.pct << "\nore " << B.result.lootOre << "\naether " << B.result.lootAether
        << "\nshards " << out.shards << "\nrating " << out.ratingDelta << "\nwin " << (B.result.win ? 1 : 0) << "\n";
    return dup(res.str());
}

char* lf_apply_defense(const char* defenderSave, long long lootOre, long long lootAether, int stars, int win, int* ratingDelta) {
    VillageState D;
    if (!loadSave(defenderSave, D)) return nullptr;
    BattleResult r;
    r.lootOre = lootOre;
    r.lootAether = lootAether;
    r.stars = stars;
    r.win = win != 0;
    int d = village::applyDefense(D, r);
    if (ratingDelta) *ratingDelta = d;
    return dup(village::save(D));
}

char* lf_auto_replay(const char* attackerSave, const char* snapshot, int seed, long long now) {
    VillageState S;
    if (!loadSave(attackerSave, S)) return nullptr;
    village::tick(S, now);
    replay::Record snap;
    if (!snapshot || !replay::load(std::string("replay 1\n") + snapshot, snap)) return nullptr;
    replay::Record rec;
    rec.opts.layout = snap.opts.layout;
    rec.opts.seed = seed;
    for (int i = 0; i < kPlayerTroops; i++) {
        if (S.army[i] > 0) rec.opts.army[(TroopType)i] = S.army[i];
        rec.opts.troopLevels[(TroopType)i] = S.troopLevel[i];
    }
    rec.opts.spells = village::spellCharges(S.level);
    rec.opts.heroLevel = village::heroUnlocked(S) ? S.heroLevel : 0;
    Battle B(rec.opts);
    int k = 0;
    for (auto [t, n] : rec.opts.army)
        for (int i = 0; i < n; i++, k++) {
            replay::Action a{0, replay::Act::Deploy, (int)t, 1.0f + (k % 20) * 0.4f, 1.0f + (k / 20) * 0.4f};
            if (replay::apply(B, a)) rec.actions.push_back(a);
        }
    const float dt = 1.f / 30.f;
    while (!B.ended && rec.steps < (int)(kBattleTime / dt)) { B.step(dt); rec.steps++; }
    return dup(replay::save(rec));
}

void lf_free(void* p) { std::free(p); }

}  // extern "C"
