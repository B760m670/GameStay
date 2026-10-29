#include "r3d.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "sokol_gfx.h"

namespace r3d {

// ---------- математика ----------
Mat4 Mat4::identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}
Mat4 Mat4::translate(Vec3 t) {
    Mat4 r = identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}
Mat4 Mat4::scale(Vec3 s) {
    Mat4 r = identity();
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}
Mat4 Mat4::rotY(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
    return r;
}
Mat4 Mat4::rotX(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
    return r;
}
Mat4 Mat4::rotZ(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
    return r;
}
Mat4 Mat4::perspective(float fovY, float aspect, float zn, float zf) {
    Mat4 r{};
    float f = 1.f / std::tan(fovY / 2);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = -1;
    r.m[14] = 2 * zf * zn / (zn - zf);
    return r;
}
Mat4 Mat4::lookAt(Vec3 eye, Vec3 at, Vec3 up) {
    Vec3 f = (at - eye).norm(), s = f.cross(up).norm(), u = s.cross(f);
    Mat4 r = identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -s.dot(eye); r.m[13] = -u.dot(eye); r.m[14] = f.dot(eye);
    return r;
}
Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r{};
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += m[k * 4 + rr] * o.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
Vec3 Mat4::point(Vec3 p) const {
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}
Vec3 Mat4::dir(Vec3 d) const {
    return {m[0] * d.x + m[4] * d.y + m[8] * d.z, m[1] * d.x + m[5] * d.y + m[9] * d.z, m[2] * d.x + m[6] * d.y + m[10] * d.z};
}

namespace {

Mat4 inverse(const Mat4& a) {
    // общий обратный 4×4 (для луча из экрана)
    const float* m = a.m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Mat4 r{};
    if (std::fabs(det) < 1e-12f) return Mat4::identity();
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] / det;
    return r;
}

// ---------- шейдеры ----------
// vs: [0..3] view-proj, [4..7] модель, [8] tint, [9] x — emissive
// fs: [0] солнце (xyz) , [1] цвет солнца, [2] небо, [3] земля, [4] туман (rgb) + near, [5] x — far, yzw — камера,
//     [6..13] огни: позиция+радиус, цвет+сила (по два vec4 на огонь)
constexpr int kVsVec4 = 10, kFsVec4 = 14;

#if defined(SOKOL_METAL)
const char* kVS = R"(
#include <metal_stdlib>
using namespace metal;
struct params_t { float4 p[10]; };
struct vs_in { float3 pos [[attribute(0)]]; float4 nrm [[attribute(1)]]; float4 color [[attribute(2)]]; };
struct vs_out { float4 pos [[position]]; float3 wpos; float3 nrm; float4 color; float emissive; };
vertex vs_out _main(vs_in in [[stage_in]], constant params_t& u [[buffer(0)]]) {
    float4x4 vp = float4x4(u.p[0], u.p[1], u.p[2], u.p[3]);
    float4x4 model = float4x4(u.p[4], u.p[5], u.p[6], u.p[7]);
    float4 w = model * float4(in.pos, 1.0);
    vs_out o;
    o.pos = vp * w;
    o.wpos = w.xyz;
    o.nrm = normalize((model * float4(in.nrm.xyz, 0.0)).xyz);
    o.color = in.color * u.p[8];
    o.emissive = u.p[9].x;
    return o;
}
)";
const char* kFS = R"(
#include <metal_stdlib>
using namespace metal;
struct params_t { float4 p[14]; };
struct vs_out { float4 pos [[position]]; float3 wpos; float3 nrm; float4 color; float emissive; };
fragment float4 _main(vs_out in [[stage_in]], constant params_t& u [[buffer(0)]]) {
    float3 n = normalize(in.nrm);
    float3 L = normalize(-u.p[0].xyz);
    float wrap = clamp((dot(n, L) + 0.25) / 1.25, 0.0, 1.0);
    float3 amb = mix(u.p[3].rgb, u.p[2].rgb, n.y * 0.5 + 0.5);
    float3 light = amb + u.p[1].rgb * wrap;
    for (int i = 0; i < 4; i++) {
        float4 lp = u.p[6 + i * 2];
        float4 lc = u.p[7 + i * 2];
        if (lp.w > 0.0) {
            float3 d = lp.xyz - in.wpos;
            float dist = length(d);
            float att = clamp(1.0 - dist / lp.w, 0.0, 1.0);
            att *= att;
            float lam = clamp(dot(n, d / max(dist, 0.001)) * 0.6 + 0.4, 0.0, 1.0);
            light += lc.rgb * lc.w * att * lam;
        }
    }
    float3 c = in.color.rgb * mix(light, float3(1.0), in.emissive);
    float3 cam = u.p[5].yzw;
    float fog = clamp((length(in.wpos - cam) - u.p[4].w) / max(u.p[5].x - u.p[4].w, 0.001), 0.0, 1.0);
    c = mix(c, u.p[4].rgb, fog * (1.0 - in.emissive * 0.7));
    return float4(c, in.color.a);
}
)";
#else
#if defined(SOKOL_GLES3)
#define LF3_HEAD "#version 300 es\nprecision highp float;\n"
#else
#define LF3_HEAD "#version 410\n"
#endif
const char* kVS = LF3_HEAD R"(
uniform vec4 vs_params[10];
layout(location=0) in vec3 a_pos;
layout(location=1) in vec4 a_nrm;
layout(location=2) in vec4 a_color;
out vec3 v_wpos;
out vec3 v_nrm;
out vec4 v_color;
out float v_emissive;
void main() {
    mat4 vp = mat4(vs_params[0], vs_params[1], vs_params[2], vs_params[3]);
    mat4 model = mat4(vs_params[4], vs_params[5], vs_params[6], vs_params[7]);
    vec4 w = model * vec4(a_pos, 1.0);
    gl_Position = vp * w;
    v_wpos = w.xyz;
    v_nrm = normalize((model * vec4(a_nrm.xyz, 0.0)).xyz);
    v_color = a_color * vs_params[8];
    v_emissive = vs_params[9].x;
}
)";
const char* kFS = LF3_HEAD R"(
uniform vec4 fs_params[14];
in vec3 v_wpos;
in vec3 v_nrm;
in vec4 v_color;
in float v_emissive;
out vec4 frag;
void main() {
    vec3 n = normalize(v_nrm);
    vec3 L = normalize(-fs_params[0].xyz);
    float wrap = clamp((dot(n, L) + 0.25) / 1.25, 0.0, 1.0);
    vec3 amb = mix(fs_params[3].rgb, fs_params[2].rgb, n.y * 0.5 + 0.5);
    vec3 light = amb + fs_params[1].rgb * wrap;
    for (int i = 0; i < 4; i++) {
        vec4 lp = fs_params[6 + i * 2];
        vec4 lc = fs_params[7 + i * 2];
        if (lp.w > 0.0) {
            vec3 d = lp.xyz - v_wpos;
            float dist = length(d);
            float att = clamp(1.0 - dist / lp.w, 0.0, 1.0);
            att *= att;
            float lam = clamp(dot(n, d / max(dist, 0.001)) * 0.6 + 0.4, 0.0, 1.0);
            light += lc.rgb * lc.w * att * lam;
        }
    }
    vec3 c = v_color.rgb * mix(light, vec3(1.0), v_emissive);
    vec3 cam = fs_params[5].yzw;
    float fog = clamp((length(v_wpos - cam) - fs_params[4].w) / max(fs_params[5].x - fs_params[4].w, 0.001), 0.0, 1.0);
    c = mix(c, fs_params[4].rgb, fog * (1.0 - v_emissive * 0.7));
    frag = vec4(c, v_color.a);
}
)";
#endif

struct DrawCmd {
    const Mesh* mesh;
    Mat4 model;
    uint32_t tint;
    float emissive;
};

struct State {
    sg_shader shader{};
    sg_pipeline pip{}, pipBlend{};
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<DrawCmd> cmds;
    Camera cam;
    Lighting light;
    Mat4 view = Mat4::identity(), proj = Mat4::identity(), vp = Mat4::identity(), invVp = Mat4::identity();
    float w = 1, h = 1;
} S;

sg_pipeline makePipeline(bool blend) {
    sg_pipeline_desc d = {};
    d.shader = S.shader;
    d.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    d.layout.attrs[1].format = SG_VERTEXFORMAT_BYTE4N;
    d.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
    d.index_type = SG_INDEXTYPE_UINT32;
    d.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    d.depth.write_enabled = !blend;
    d.cull_mode = SG_CULLMODE_BACK;
    d.face_winding = SG_FACEWINDING_CCW;
    if (blend) {
        d.colors[0].blend.enabled = true;
        d.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        d.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        d.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ZERO;  // прозрачность кадра не трогаем
        d.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE;
    }
    d.label = blend ? "r3d-blend" : "r3d-opaque";
    return sg_make_pipeline(&d);
}

int8_t snorm(float v) { return (int8_t)std::lround(std::clamp(v, -1.f, 1.f) * 127.f); }

}  // namespace

// ---------- сборка тел ----------
void Builder::tri(Vec3 a, Vec3 b, Vec3 c, uint32_t color) {
    Vec3 n = (b - a).cross(c - a).norm();
    uint32_t base = (uint32_t)verts.size();
    for (Vec3 p : {a, b, c}) verts.push_back({p.x, p.y, p.z, snorm(n.x), snorm(n.y), snorm(n.z), 0, color});
    idx.insert(idx.end(), {base, base + 1, base + 2});
}

void Builder::quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t color) {
    Vec3 n = (b - a).cross(d - a).norm();
    uint32_t base = (uint32_t)verts.size();
    for (Vec3 p : {a, b, c, d}) verts.push_back({p.x, p.y, p.z, snorm(n.x), snorm(n.y), snorm(n.z), 0, color});
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void Builder::box(Vec3 c, Vec3 h, uint32_t color, const Mat4& xf) {
    Vec3 p[8];
    for (int i = 0; i < 8; i++) p[i] = xf.point({c.x + ((i & 1) ? h.x : -h.x), c.y + ((i & 2) ? h.y : -h.y), c.z + ((i & 4) ? h.z : -h.z)});
    // вершины: бит0 — x, бит1 — y, бит2 — z; грани против часовой снаружи
    quad(p[0], p[4], p[6], p[2], color);  // -x
    quad(p[1], p[3], p[7], p[5], color);  // +x
    quad(p[0], p[1], p[5], p[4], color);  // -y
    quad(p[2], p[6], p[7], p[3], color);  // +y
    quad(p[0], p[2], p[3], p[1], color);  // -z
    quad(p[4], p[5], p[7], p[6], color);  // +z
}

void Builder::cylinder(Vec3 base, float r0, float r1, float h, uint32_t color, int segs, const Mat4& xf, bool flat) {
    std::vector<Vec3> bot(segs), top(segs);
    for (int i = 0; i < segs; i++) {
        float a = (float)i / segs * 6.2831853f;
        float ca = std::cos(a), sa = std::sin(a);
        bot[i] = xf.point({base.x + ca * r0, base.y, base.z + sa * r0});
        top[i] = xf.point({base.x + ca * r1, base.y + h, base.z + sa * r1});
    }
    Vec3 cb = xf.point(base), ct = xf.point({base.x, base.y + h, base.z});
    float slope = (r0 - r1) / std::max(h, 1e-4f);
    for (int i = 0; i < segs; i++) {
        int j = (i + 1) % segs;
        if (flat) {
            quad(bot[i], top[i], top[j], bot[j], color);
        } else {  // гладкие бока: нормаль по радиусу с учётом наклона
            uint32_t b0 = (uint32_t)verts.size();
            float a0 = (float)i / segs * 6.2831853f, a1 = (float)j / segs * 6.2831853f;
            Vec3 n0 = xf.dir(Vec3(std::cos(a0), slope, std::sin(a0)).norm()).norm();
            Vec3 n1 = xf.dir(Vec3(std::cos(a1), slope, std::sin(a1)).norm()).norm();
            auto push = [&](Vec3 p, Vec3 n) { verts.push_back({p.x, p.y, p.z, snorm(n.x), snorm(n.y), snorm(n.z), 0, color}); };
            push(bot[i], n0); push(top[i], n0); push(top[j], n1); push(bot[j], n1);
            idx.insert(idx.end(), {b0, b0 + 1, b0 + 2, b0, b0 + 2, b0 + 3});
        }
        if (r1 > 0) tri(ct, top[j], top[i], color);
        if (r0 > 0) tri(cb, bot[i], bot[j], color);
    }
}

void Builder::sphere(Vec3 c, Vec3 r, uint32_t color, int segs, int rings, const Mat4& xf) {
    uint32_t base = (uint32_t)verts.size();
    for (int y = 0; y <= rings; y++) {
        float v = (float)y / rings * 3.14159265f;
        for (int x = 0; x <= segs; x++) {
            float u = (float)x / segs * 6.2831853f;
            Vec3 n(std::sin(v) * std::cos(u), std::cos(v), std::sin(v) * std::sin(u));
            Vec3 p = xf.point({c.x + n.x * r.x, c.y + n.y * r.y, c.z + n.z * r.z});
            Vec3 wn = xf.dir({n.x / r.x, n.y / r.y, n.z / r.z}).norm();
            verts.push_back({p.x, p.y, p.z, snorm(wn.x), snorm(wn.y), snorm(wn.z), 0, color});
        }
    }
    for (int y = 0; y < rings; y++)
        for (int x = 0; x < segs; x++) {
            uint32_t a = base + y * (segs + 1) + x, b = a + segs + 1;
            idx.insert(idx.end(), {a, a + 1, b + 1, a, b + 1, b});
        }
}

void Builder::wedge(Vec3 c, Vec3 h, uint32_t color, const Mat4& xf) {
    auto P = [&](float x, float y, float z) { return xf.point({c.x + x, c.y + y, c.z + z}); };
    Vec3 a = P(-h.x, -h.y, -h.z), b = P(h.x, -h.y, -h.z), cc = P(h.x, -h.y, h.z), d = P(-h.x, -h.y, h.z);
    Vec3 e = P(-h.x, h.y, 0), f = P(h.x, h.y, 0);
    quad(a, b, cc, d, color);      // низ (смотрит вниз)
    quad(a, e, f, b, color);       // скат к -z
    quad(d, cc, f, e, color);      // скат к +z
    tri(a, d, e, color);           // торцы
    tri(b, f, cc, color);
}

Mesh* Builder::build() {
    auto m = std::make_unique<Mesh>();
    if (idx.empty()) return nullptr;
    sg_buffer_desc vb = {};
    vb.usage.vertex_buffer = true;
    vb.data.ptr = verts.data();
    vb.data.size = verts.size() * sizeof(Vtx);
    m->vbuf = sg_make_buffer(&vb).id;
    sg_buffer_desc ib = {};
    ib.usage.index_buffer = true;
    ib.data.ptr = idx.data();
    ib.data.size = idx.size() * sizeof(uint32_t);
    m->ibuf = sg_make_buffer(&ib).id;
    m->count = (int)idx.size();
    Vec3 lo(1e9f, 1e9f, 1e9f), hi(-1e9f, -1e9f, -1e9f);
    for (const Vtx& v : verts) {
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    m->bmin = lo;
    m->bmax = hi;
    verts.clear();
    idx.clear();
    S.meshes.push_back(std::move(m));
    return S.meshes.back().get();
}

// ---------- кадр ----------
bool init() {
    sg_shader_desc sd = {};
    sd.vertex_func.source = kVS;
    sd.fragment_func.source = kFS;
    sd.attrs[0].glsl_name = "a_pos";
    sd.attrs[1].glsl_name = "a_nrm";
    sd.attrs[2].glsl_name = "a_color";
    for (int i = 0; i < 3; i++) sd.attrs[i].base_type = SG_SHADERATTRBASETYPE_FLOAT;
    sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    sd.uniform_blocks[0].size = kVsVec4 * 16;
    sd.uniform_blocks[0].msl_buffer_n = 0;
    sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    sd.uniform_blocks[0].glsl_uniforms[0].array_count = kVsVec4;
    sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "vs_params";
    sd.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
    sd.uniform_blocks[1].size = kFsVec4 * 16;
    sd.uniform_blocks[1].msl_buffer_n = 0;
    sd.uniform_blocks[1].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    sd.uniform_blocks[1].glsl_uniforms[0].array_count = kFsVec4;
    sd.uniform_blocks[1].glsl_uniforms[0].glsl_name = "fs_params";
    sd.label = "r3d-shader";
    S.shader = sg_make_shader(&sd);
    S.pip = makePipeline(false);
    S.pipBlend = makePipeline(true);
    return sg_query_shader_state(S.shader) == SG_RESOURCESTATE_VALID;
}

void beginFrame(const Camera& cam, const Lighting& light, float width, float height) {
    S.cam = cam;
    S.light = light;
    S.w = std::max(1.f, width);
    S.h = std::max(1.f, height);
    S.view = Mat4::lookAt(cam.eye, cam.target, {0, 1, 0});
    S.proj = Mat4::perspective(cam.fovY, S.w / S.h, cam.zNear, cam.zFar);
    S.vp = S.proj * S.view;
    S.invVp = inverse(S.vp);
    S.cmds.clear();
}

void draw(const Mesh* mesh, const Mat4& model, uint32_t tint, float emissive) {
    if (mesh) S.cmds.push_back({mesh, model, tint, emissive});
}

void flush() {
    if (S.cmds.empty()) return;
    float fs[kFsVec4 * 4] = {};
    const Lighting& L = S.light;
    auto put = [&](int i, Vec3 v, float w) { fs[i * 4] = v.x; fs[i * 4 + 1] = v.y; fs[i * 4 + 2] = v.z; fs[i * 4 + 3] = w; };
    put(0, L.sunDir.norm(), 0);
    put(1, L.sunColor, 0);
    put(2, L.skyColor, 0);
    put(3, L.groundColor, 0);
    put(4, L.fogColor, L.fogNear);
    fs[20] = L.fogFar; fs[21] = S.cam.eye.x; fs[22] = S.cam.eye.y; fs[23] = S.cam.eye.z;
    for (int i = 0; i < 4; i++) {
        put(6 + i * 2, L.lights[i].pos, L.lights[i].radius);
        put(7 + i * 2, L.lights[i].color, L.lights[i].intensity);
    }
    // сначала непрозрачное, потом полупрозрачное (дальнее раньше)
    std::stable_sort(S.cmds.begin(), S.cmds.end(), [](const DrawCmd& a, const DrawCmd& b) {
        bool ta = (a.tint >> 24) < 255, tb = (b.tint >> 24) < 255;
        return ta < tb;
    });
    bool blendOn = false, first = true;
    for (const DrawCmd& c : S.cmds) {
        bool blend = (c.tint >> 24) < 255;
        if (first || blend != blendOn) {
            sg_apply_pipeline(blend ? S.pipBlend : S.pip);
            sg_range fr = {fs, sizeof(fs)};
            sg_apply_uniforms(1, &fr);
            blendOn = blend;
            first = false;
        }
        float vs[kVsVec4 * 4];
        std::memcpy(vs, S.vp.m, 64);
        std::memcpy(vs + 16, c.model.m, 64);
        for (int k = 0; k < 4; k++) vs[32 + k] = ((c.tint >> (k * 8)) & 255) / 255.f;
        vs[36] = c.emissive; vs[37] = vs[38] = vs[39] = 0;
        sg_range vr = {vs, sizeof(vs)};
        sg_apply_uniforms(0, &vr);
        sg_bindings b = {};
        b.vertex_buffers[0].id = c.mesh->vbuf;
        b.index_buffer.id = c.mesh->ibuf;
        sg_apply_bindings(&b);
        sg_draw(0, c.mesh->count, 1);
    }
    S.cmds.clear();
}

bool project(Vec3 p, float& sx, float& sy) {
    const float* m = S.vp.m;
    float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    float y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (w <= 0.01f) return false;
    sx = (x / w * 0.5f + 0.5f) * S.w;
    sy = (1 - (y / w * 0.5f + 0.5f)) * S.h;
    return true;
}

bool unprojectToGround(float sx, float sy, float h, Vec3& out) {
    float nx = sx / S.w * 2 - 1, ny = 1 - sy / S.h * 2;
    auto un = [&](float z) {
        const float* m = S.invVp.m;
        float x = m[0] * nx + m[4] * ny + m[8] * z + m[12];
        float y = m[1] * nx + m[5] * ny + m[9] * z + m[13];
        float zz = m[2] * nx + m[6] * ny + m[10] * z + m[14];
        float w = m[3] * nx + m[7] * ny + m[11] * z + m[15];
        return Vec3(x / w, y / w, zz / w);
    };
    Vec3 a = un(-1), b = un(1), d = b - a;
    if (std::fabs(d.y) < 1e-6f) return false;
    float t = (h - a.y) / d.y;
    if (t < 0) return false;
    out = a + d * t;
    return true;
}

const Mat4& viewProj() { return S.vp; }
const Camera& camera() { return S.cam; }

}  // namespace r3d
