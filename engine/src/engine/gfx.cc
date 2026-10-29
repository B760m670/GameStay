#include "gfx.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "../platform/platform.h"
#include "r3d.h"

#if defined(SOKOL_GLCORE)
#include <GL/gl.h>
#endif

namespace gfx {
namespace {

constexpr int kMaxQuads = 60000;

#if defined(SOKOL_METAL)
const char* kVS = R"(
#include <metal_stdlib>
using namespace metal;
struct params_t { float4 screen; };
struct vs_in { float2 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; float4 color [[attribute(2)]]; };
struct vs_out { float4 pos [[position]]; float2 uv; float4 color; };
vertex vs_out _main(vs_in in [[stage_in]], constant params_t& p [[buffer(0)]]) {
    vs_out o;
    o.pos = float4(in.pos.x * p.screen.x - 1.0, in.pos.y * p.screen.y + 1.0, 0.0, 1.0);
    o.uv = in.uv;
    o.color = in.color;
    return o;
}
)";
const char* kFS = R"(
#include <metal_stdlib>
using namespace metal;
struct vs_out { float4 pos [[position]]; float2 uv; float4 color; };
fragment float4 _main(vs_out in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {
    return tex.sample(smp, in.uv) * in.color;
}
)";
#else
// GLSL: на ПК — 4.1 core, в браузере (WebGL2) — ES 3.0 с явной точностью.
#if defined(SOKOL_GLES3)
#define LF_GLSL_HEAD "#version 300 es\nprecision mediump float;\n"
#else
#define LF_GLSL_HEAD "#version 410\n"
#endif
const char* kVS = LF_GLSL_HEAD R"(
uniform vec4 u_screen;
layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_color;
out vec2 v_uv;
out vec4 v_color;
void main() {
    gl_Position = vec4(a_pos.x * u_screen.x - 1.0, a_pos.y * u_screen.y + 1.0, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
}
)";
const char* kFS = LF_GLSL_HEAD R"(
uniform sampler2D tex;
in vec2 v_uv;
in vec4 v_color;
out vec4 frag;
void main() { frag = texture(tex, v_uv) * v_color; }
)";
#endif

struct Cmd {
    const Texture* tex;
    Blend blend;
    int firstQuad, quads;
};

struct State {
    sg_shader shader{};
    sg_pipeline pipAlpha{}, pipAdd{};
    sg_buffer vbuf{}, ibuf{};
    sg_sampler sampler{};
    std::vector<Vtx> verts;
    std::vector<Cmd> cmds;
    std::vector<std::unique_ptr<Texture>> textures;
    Texture* whiteTex = nullptr;
    Blend blend = Blend::Alpha;
    float w = 1, h = 1;
    struct Xf { float cx, cy, s, a; };
    std::vector<Xf> xf;
} S;

sg_pipeline makePipeline(Blend b) {
    sg_pipeline_desc d = {};
    d.shader = S.shader;
    d.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    d.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    d.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
    d.index_type = SG_INDEXTYPE_UINT32;
    d.colors[0].blend.enabled = true;
    d.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    d.colors[0].blend.dst_factor_rgb = b == Blend::Add ? SG_BLENDFACTOR_ONE : SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    d.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    d.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    d.label = b == Blend::Add ? "sprites-add" : "sprites-alpha";
    return sg_make_pipeline(&d);
}

}  // namespace

uint32_t lerpColor(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int i = 0; i < 4; i++) {
        float ca = (float)((a >> (i * 8)) & 255), cb = (float)((b >> (i * 8)) & 255);
        int c = (int)(ca + (cb - ca) * t + 0.5f);
        out |= (uint32_t)std::clamp(c, 0, 255) << (i * 8);
    }
    return out;
}

bool init() {
    sg_desc desc = {};
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);
    if (!sg_isvalid()) return false;

    sg_shader_desc sd = {};
    sd.vertex_func.source = kVS;
    sd.fragment_func.source = kFS;
    sd.attrs[0].glsl_name = "a_pos";
    sd.attrs[1].glsl_name = "a_uv";
    sd.attrs[2].glsl_name = "a_color";
    sd.attrs[0].base_type = SG_SHADERATTRBASETYPE_FLOAT;
    sd.attrs[1].base_type = SG_SHADERATTRBASETYPE_FLOAT;
    sd.attrs[2].base_type = SG_SHADERATTRBASETYPE_FLOAT;
    sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    sd.uniform_blocks[0].size = 16;
    sd.uniform_blocks[0].msl_buffer_n = 0;
    sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "u_screen";
    sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
    sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
    sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
    sd.views[0].texture.msl_texture_n = 0;
    sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
    sd.samplers[0].msl_sampler_n = 0;
    sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.texture_sampler_pairs[0].view_slot = 0;
    sd.texture_sampler_pairs[0].sampler_slot = 0;
    sd.texture_sampler_pairs[0].glsl_name = "tex";
    sd.label = "sprite-shader";
    S.shader = sg_make_shader(&sd);
    S.pipAlpha = makePipeline(Blend::Alpha);
    S.pipAdd = makePipeline(Blend::Add);

    sg_buffer_desc vb = {};
    vb.size = sizeof(Vtx) * 4 * kMaxQuads;
    vb.usage.vertex_buffer = true;
    vb.usage.write_transient = true;
    vb.label = "sprite-verts";
    S.vbuf = sg_make_buffer(&vb);

    std::vector<uint32_t> idx(6 * kMaxQuads);
    for (uint32_t q = 0; q < (uint32_t)kMaxQuads; q++) {
        uint32_t b = q * 4;
        uint32_t* p = &idx[q * 6];
        p[0] = b; p[1] = b + 1; p[2] = b + 2; p[3] = b; p[4] = b + 2; p[5] = b + 3;
    }
    sg_buffer_desc ib = {};
    ib.usage.index_buffer = true;
    ib.data.ptr = idx.data();
    ib.data.size = idx.size() * sizeof(uint32_t);
    ib.label = "sprite-indices";
    S.ibuf = sg_make_buffer(&ib);

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.mipmap_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    S.sampler = sg_make_sampler(&smp);

    uint8_t px[4] = {255, 255, 255, 255};
    S.whiteTex = createTexture(1, 1, px, false);
    S.verts.reserve(4 * 4096);
    return true;
}

void shutdown() { sg_shutdown(); }

Texture* createTexture(int w, int h, const uint8_t* rgba, bool mipmaps) {
    // Мипмапы считаем на CPU (бокс-фильтр с учётом альфы) — спрайты не мерцают при отдалении.
    std::vector<std::vector<uint8_t>> levels;
    levels.emplace_back(rgba, rgba + (size_t)w * h * 4);
    int lw = w, lh = h;
    while (mipmaps && (lw > 1 || lh > 1) && levels.size() < 12) {
        int nw = std::max(1, lw / 2), nh = std::max(1, lh / 2);
        const auto& src = levels.back();
        std::vector<uint8_t> dst((size_t)nw * nh * 4);
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++) {
                float acc[4] = {0, 0, 0, 0};
                float wsum = 0;
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++) {
                        int sx = std::min(lw - 1, x * 2 + dx), sy = std::min(lh - 1, y * 2 + dy);
                        const uint8_t* p = &src[((size_t)sy * lw + sx) * 4];
                        float a = p[3] / 255.f;
                        acc[0] += p[0] * a; acc[1] += p[1] * a; acc[2] += p[2] * a; acc[3] += p[3];
                        wsum += a;
                    }
                uint8_t* o = &dst[((size_t)y * nw + x) * 4];
                for (int c = 0; c < 3; c++) o[c] = (uint8_t)(wsum > 0 ? acc[c] / wsum : 0);
                o[3] = (uint8_t)(acc[3] / 4);
            }
        levels.push_back(std::move(dst));
        lw = nw; lh = nh;
    }
    sg_image_desc d = {};
    d.width = w;
    d.height = h;
    d.pixel_format = SG_PIXELFORMAT_RGBA8;
    d.num_mipmaps = (int)levels.size();
    for (size_t i = 0; i < levels.size(); i++) {
        d.data.mip_levels[i].ptr = levels[i].data();
        d.data.mip_levels[i].size = levels[i].size();
    }
    sg_image img = sg_make_image(&d);
    sg_view_desc vd = {};
    vd.texture.image = img;
    sg_view view = sg_make_view(&vd);
    auto t = std::make_unique<Texture>();
    t->img = img.id;
    t->view = view.id;
    t->w = w;
    t->h = h;
    S.textures.push_back(std::move(t));
    return S.textures.back().get();
}

Texture* loadTexture(const std::string& path, bool mipmaps) {
    std::vector<unsigned char> data;
    if (!platform::readFile(path, data)) {
        std::fprintf(stderr, "[gfx] cannot read %s\n", path.c_str());
        return nullptr;
    }
    int w, h, n;
    unsigned char* px = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &n, 4);
    if (!px) return nullptr;
    Texture* t = createTexture(w, h, px, mipmaps);
    stbi_image_free(px);
    return t;
}

Texture* white() { return S.whiteTex; }

void beginFrame(float width, float height) {
    S.w = width;
    S.h = height;
    S.verts.clear();
    S.cmds.clear();
    S.blend = Blend::Alpha;
    S.xf.clear();
}
float width() { return S.w; }
float height() { return S.h; }

void setBlend(Blend b) { S.blend = b; }

void pushScale(float cx, float cy, float s, float a) { S.xf.push_back({cx, cy, s, a}); }
void popScale() { if (!S.xf.empty()) S.xf.pop_back(); }

void quad(const Texture* t, const Vtx vin[4]) {
    if (!t || S.verts.size() / 4 >= (size_t)kMaxQuads) return;
    Vtx v[4] = {vin[0], vin[1], vin[2], vin[3]};
    for (auto it = S.xf.rbegin(); it != S.xf.rend(); ++it)
        for (auto& p : v) {
            p.x = it->cx + (p.x - it->cx) * it->s;
            p.y = it->cy + (p.y - it->cy) * it->s;
            if (it->a < 1.f) p.color = withAlpha(p.color, it->a);
        }
    if (S.cmds.empty() || S.cmds.back().tex != t || S.cmds.back().blend != S.blend) {
        S.cmds.push_back({t, S.blend, (int)(S.verts.size() / 4), 0});
    }
    S.verts.insert(S.verts.end(), v, v + 4);
    S.cmds.back().quads++;
}

void rect(const Texture* t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color) {
    Vtx v[4] = {{x, y, u0, v0, color}, {x + w, y, u1, v0, color}, {x + w, y + h, u1, v1, color}, {x, y + h, u0, v1, color}};
    quad(t, v);
}

void fillRect(float x, float y, float w, float h, uint32_t color) { rect(S.whiteTex, x, y, w, h, 0, 0, 1, 1, color); }

void endFrame(uint32_t clear) {
    sg_pass pass = {};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = {(clear & 255) / 255.f, ((clear >> 8) & 255) / 255.f, ((clear >> 16) & 255) / 255.f, 1.f};
    pass.swapchain = sglue_swapchain();
    if (!S.verts.empty()) {
        sg_write_buffer_desc wd = {};
        wd.src.data.ptr = S.verts.data();
        wd.src.data.size = S.verts.size() * sizeof(Vtx);
        wd.dst.buffer = S.vbuf;
        sg_write_buffer_transient(&wd);
    }
    sg_begin_pass(&pass);
    r3d::flush();  // мир в 3D — под интерфейсом
    float ub[4] = {2.f / S.w, -2.f / S.h, 0, 0};
    Blend current = Blend::Alpha;
    bool first = true;
    for (const Cmd& c : S.cmds) {
        if (first || c.blend != current) {
            sg_apply_pipeline(c.blend == Blend::Add ? S.pipAdd : S.pipAlpha);
            sg_range r = {ub, sizeof(ub)};
            sg_apply_uniforms(0, &r);
            current = c.blend;
            first = false;
        }
        sg_bindings b = {};
        b.vertex_buffers[0] = S.vbuf;
        b.index_buffer = S.ibuf;
        b.views[0].id = c.tex->view;
        b.samplers[0] = S.sampler;
        sg_apply_bindings(&b);
        sg_draw(c.firstQuad * 6, c.quads * 6, 1);
    }
    sg_end_pass();
    sg_commit();
}

bool screenshot(const std::string& path) {
#if defined(SOKOL_GLCORE)
    int w = sapp_width(), h = sapp_height();
    std::vector<uint8_t> px((size_t)w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;  // кадр на экране непрозрачен
    stbi_flip_vertically_on_write(1);
    return stbi_write_png(path.c_str(), w, h, 4, px.data(), w * 4) != 0;
#else
    (void)path;
    return false;
#endif
}

}  // namespace gfx
