// Процедурная генерация чужих острогов и волн набега разбойников.
#include <algorithm>
#include <cmath>
#include <optional>

#include "core.h"

namespace lf::basegen {

std::string rivalName(Rng& rng) {
    static const char* A[] = {"Кривой", "Рыжий", "Лихой", "Беглый", "Старый", "Хмурый", "Тихий", "Косой", "Седой", "Хромой"};
    static const char* N[] = {"Ерофей", "Пахом", "Гордей", "Фрол", "Демид", "Остап", "Прохор", "Евсей", "Никифор", "Савва", "Тимофей", "Лука"};
    std::string a = A[rng.range(10)];
    return a + " " + N[rng.range(12)];
}

std::vector<LayoutItem> generate(int LL, int seed, const std::map<BType, int>* counts, float richness, int levelBias) {
    LL = std::clamp(LL, 1, kMaxLantern);  // походы с острогом выше предела игры строятся на пределе
    Rng rng(seed);
    std::vector<LayoutItem> layout;
    std::vector<uint8_t> occ(kGrid * kGrid, 0);  // 0 пусто, 1 здание, 2 стена
    const int C = kGrid / 2;
    const float lr = std::min(19.f, 11.f + 2.f * LL);  // радиус поселения: растёт с уровнем острога
    auto inside = [&](int x, int y, int s) {  // вся постройка в круге поселения
        for (int j : {y, y + s}) for (int i : {x, x + s})
            if (std::hypot(i - (float)C, j - (float)C) > lr + 0.5f) return false;
        return true;
    };

    auto lvl = [&](BType t) {
        int max = village::maxLevelAllowed(t, LL);
        if (max <= 0) return 1;
        int l = max - (rng.next() < 0.4 ? 1 : 0) + levelBias;
        return std::max(1, std::min({B(t).maxLevel, max, l}));
    };
    auto count = [&](BType t) {
        if (counts) {
            auto it = counts->find(t);
            if (it != counts->end()) return it->second;
        }
        return limit(t, LL);
    };
    auto freeAt = [&](int x, int y, int s, int margin) {
        if (x < kBorder || y < kBorder || x + s > kGrid - kBorder || y + s > kGrid - kBorder) return false;
        for (int j = y - margin; j < y + s + margin; j++)
            for (int i = x - margin; i < x + s + margin; i++) {
                if (i < 0 || j < 0 || i >= kGrid || j >= kGrid) continue;
                bool inside = i >= x && i < x + s && j >= y && j < y + s;
                uint8_t v = occ[j * kGrid + i];
                if (v == 1 || (inside && v == 2)) return false;
            }
        return true;
    };
    auto put = [&](BType t, int x, int y, int level) -> size_t {
        int s = B(t).size;
        for (int j = y; j < y + s; j++)
            for (int i = x; i < x + s; i++) occ[j * kGrid + i] = t == BType::Wall ? 2 : 1;
        LayoutItem it{t, x, y, level, (int)layout.size()};
        layout.push_back(it);
        return layout.size() - 1;
    };

    size_t lantern = put(BType::Lantern, C - 2, C - 2, LL);

    // Частокол — квадратные кольца вокруг острога.
    int walls = count(BType::Wall);
    int wallLevel = lvl(BType::Wall);
    const int lx = layout[lantern].x, ly = layout[lantern].y;
    auto tryRing = [&](int k) {
        int x0 = lx - k, y0 = ly - k, x1 = lx + 3 + k, y1 = ly + 3 + k;
        std::vector<std::pair<int, int>> all;
        for (int i = x0; i <= x1; i++) { all.push_back({i, y0}); all.push_back({i, y1}); }
        for (int j = y0 + 1; j < y1; j++) { all.push_back({x0, j}); all.push_back({x1, j}); }
        for (auto [i, j] : all) if (!inside(i, j, 1)) return false;
        std::vector<std::pair<int, int>> tiles;
        for (auto p : all) if (freeAt(p.first, p.second, 1, 0)) tiles.push_back(p);
        if (tiles.empty() || (int)tiles.size() > walls) return false;
        for (auto [i, j] : tiles) put(BType::Wall, i, j, wallLevel);
        walls -= (int)tiles.size();
        return true;
    };
    std::vector<int> rings;
    if (walls >= 100 && tryRing(3)) rings.push_back(3);
    for (int k = 9; k >= 1; k--) {
        bool near = false;
        for (int r : rings) if (std::abs(r - k) < 3) near = true;
        if (near) continue;
        if (tryRing(k)) { rings.push_back(k); break; }
    }

    // Остальные постройки: 0 — ближе к центру, 1 — оборона по поясам, 2 — окраина.
    const std::pair<BType, int> order[] = {
        {BType::OreStore, 0}, {BType::AetherStore, 0},
        {BType::Prism, 1}, {BType::Catapult, 1}, {BType::Brazier, 1}, {BType::Ballista, 1},
        {BType::Barracks, 2}, {BType::Camp, 2}, {BType::OreMine, 2}, {BType::AetherWell, 2}, {BType::BuilderHut, 2}, {BType::Beacon, 2}, {BType::Workshop, 2},
        {BType::Trap, 1},
    };
    std::map<BType, std::vector<size_t>> placed;
    for (auto [type, role] : order) {
        int n = count(type), s = B(type).size;
        for (int k = 0; k < n; k++) {
            std::optional<std::pair<int, int>> best;
            double bestScore = 1e18;
            for (int c = 0; c < 260; c++) {
                int x = (int)std::floor(kBorder + rng.next() * (kGrid - 2 * kBorder - s));
                int y = (int)std::floor(kBorder + rng.next() * (kGrid - 2 * kBorder - s));
                if (!freeAt(x, y, s, 1) || !inside(x, y, s)) continue;
                double d = std::hypot(x + s / 2.0 - C, y + s / 2.0 - C), score;
                if (role == 0) score = d;
                else if (role == 1) score = std::abs(d - (3 + std::fmod(k * 2.7, lr + 1))) * 0.7;
                else score = -d * 0.6;
                for (size_t oi : placed[type]) {
                    double od = std::hypot(layout[oi].x - x, layout[oi].y - y);
                    if (od < 7) score += (7 - od) * 1.2;
                }
                score += rng.next() * 2.5;
                if (score < bestScore) { bestScore = score; best = std::make_pair(x, y); }
            }
            if (!best) continue;
            placed[type].push_back(put(type, best->first, best->second, type == BType::BuilderHut ? 1 : lvl(type)));
        }
    }

    // Добыча.
    float pool = kBaseLoot[LL - 1] * richness;
    auto share = [&](BType t, float frac, bool ore) {
        int n = 0;
        for (auto& b : layout) if (b.type == t) n++;
        for (auto& b : layout)
            if (b.type == t) (ore ? b.lootOre : b.lootAether) += pool * frac / n;
    };
    share(BType::OreStore, 0.6f, true);
    share(BType::OreMine, 0.25f, true);
    share(BType::AetherStore, 0.6f, false);
    share(BType::AetherWell, 0.25f, false);
    layout[lantern].lootOre = pool * 0.15f;
    layout[lantern].lootAether = pool * 0.15f;
    return layout;
}

Night nightWaves(int LL, int nights, int seed) {
    Rng rng(seed);
    int budget = 10 + 14 * (LL - 1) + 2 * std::min(nights, 20);
    std::vector<TroopType> types;
    for (TroopType t : {TroopType::Morok, TroopType::Brute, TroopType::Leech, TroopType::Wraith})
        if (T(t).unlock <= LL) types.push_back(t);
    Night n;
    n.waves = {Wave{2}, Wave{22}, Wave{45}};
    const float shares[3] = {0.3f, 0.35f, 0.35f};
    for (int i = 0; i < 3; i++) {
        int left = (int)std::lround(budget * shares[i]);
        while (left > 0) {
            std::vector<TroopType> opts;
            for (auto t : types) if (T(t).housing <= left) opts.push_back(t);
            if (opts.empty()) break;
            TroopType t = rng.next() < 0.45 ? TroopType::Morok : opts[rng.range((int)opts.size())];
            if (T(t).housing > left) break;
            n.waves[i].units[t]++;
            left -= T(t).housing;
        }
    }
    n.raidMult = 1.f + 0.05f * std::min(nights, 30);
    return n;
}

}  // namespace lf::basegen
