#include "font.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "stb_truetype.h"
#include "../platform/platform.h"

namespace font {
namespace {

struct Range { int first, count; };
const Range kRanges[] = {{32, 95}, {0xA0, 96}, {0x400, 96}, {0x2010, 24}, {0x2190, 4}, {0x2605, 1}};
constexpr int kAtlasW = 2048, kAtlasH = 1024;

struct State {
    std::vector<unsigned char> ttf;
    std::map<int, stbtt_packedchar> glyphs;
    gfx::Texture* tex = nullptr;
    float bakePx = 56, ascent = 0, descent = 0, lineGap = 0;
} S;

int decode(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : 1;
    int cp = c & (0x3F >> n);
    for (int k = 0; k < n && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
}

}  // namespace

bool load(const std::string& ttfPath, float bakePx) {
    if (!platform::readFile(ttfPath, S.ttf)) {
        std::fprintf(stderr, "[font] cannot read %s\n", ttfPath.c_str());
        return false;
    }
    S.bakePx = bakePx;
    std::vector<unsigned char> alpha(kAtlasW * kAtlasH);
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, alpha.data(), kAtlasW, kAtlasH, 0, 2, nullptr);
    stbtt_PackSetOversampling(&pc, 1, 1);
    for (const Range& r : kRanges) {
        std::vector<stbtt_packedchar> pcs(r.count);
        stbtt_PackFontRange(&pc, S.ttf.data(), 0, bakePx, r.first, r.count, pcs.data());
        for (int i = 0; i < r.count; i++) S.glyphs[r.first + i] = pcs[i];
    }
    stbtt_PackEnd(&pc);
    stbtt_fontinfo info;
    stbtt_InitFont(&info, S.ttf.data(), 0);
    int a, d, g;
    stbtt_GetFontVMetrics(&info, &a, &d, &g);
    float sc = stbtt_ScaleForPixelHeight(&info, bakePx);
    S.ascent = a * sc;
    S.descent = d * sc;
    S.lineGap = g * sc;
    std::vector<unsigned char> rgba(kAtlasW * kAtlasH * 4);
    for (int i = 0; i < kAtlasW * kAtlasH; i++) {
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha[i];
    }
    S.tex = gfx::createTexture(kAtlasW, kAtlasH, rgba.data(), true);
    return true;
}

float lineHeight(float size) { return (S.ascent - S.descent + S.lineGap) / S.bakePx * size; }

float measure(const std::string& text, float size) {
    float k = size / S.bakePx, w = 0;
    for (size_t i = 0; i < text.size();) {
        int cp = decode(text, i);
        auto it = S.glyphs.find(cp);
        if (it == S.glyphs.end()) it = S.glyphs.find('?');
        if (it != S.glyphs.end()) w += it->second.xadvance * k;
    }
    return w;
}

static void drawRun(const std::string& text, float x, float y, float size, uint32_t color) {
    float k = size / S.bakePx;
    float baseline = y + S.ascent * k;
    for (size_t i = 0; i < text.size();) {
        int cp = decode(text, i);
        auto it = S.glyphs.find(cp);
        if (it == S.glyphs.end()) it = S.glyphs.find('?');
        if (it == S.glyphs.end()) continue;
        const stbtt_packedchar& g = it->second;
        float x0 = x + g.xoff * k, y0 = baseline + g.yoff * k, x1 = x + g.xoff2 * k, y1 = baseline + g.yoff2 * k;
        if (g.x1 > g.x0)
            gfx::rect(S.tex, std::round(x0 * 2) / 2, std::round(y0 * 2) / 2, x1 - x0, y1 - y0, g.x0 / (float)kAtlasW, g.y0 / (float)kAtlasH,
                      g.x1 / (float)kAtlasW, g.y1 / (float)kAtlasH, color);
        x += g.xadvance * k;
    }
}

float draw(const std::string& text, float x, float y, float size, uint32_t color, Align align, bool outline) {
    if (!S.tex) return 0;
    float w = measure(text, size);
    if (align == Center) x -= w / 2;
    else if (align == Right) x -= w;
    if (outline) {
        // мультяшная обводка как в мобильных стратегиях
        float o = std::max(1.f, size * 0.08f);
        uint32_t oc = gfx::rgba(20, 12, 30, (int)(((color >> 24) & 255) * 0.9f));
        const float dirs[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-0.7f, -0.7f}, {0.7f, -0.7f}, {-0.7f, 0.7f}, {0.7f, 0.7f}};
        for (auto& d : dirs) drawRun(text, x + d[0] * o, y + d[1] * o + o * 0.6f, size, oc);
    }
    drawRun(text, x, y, size, color);
    return w;
}

std::vector<std::string> wrap(const std::string& text, float size, float maxWidth) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flushWord = [&] {
        if (word.empty()) return;
        std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure(cand, size) > maxWidth) { lines.push_back(line); line = word; }
        else line = cand;
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') flushWord();
        else if (c == '\n') { flushWord(); lines.push_back(line); line.clear(); }
        else word += c;
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

}  // namespace font
