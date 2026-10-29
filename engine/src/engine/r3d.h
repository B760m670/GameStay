#pragma once
// 3D-рендер мира из простых тел: бруски, цилиндры, конусы, шары, клинья — как детали в Roblox. Цвет в вершинах,
// освещение — солнце, небо, туман и до четырёх точечных огней (костёр, факелы). Рисуется в том же проходе, что и
// 2D-интерфейс, перед ним (gfx::endFrame вызывает r3d::flush).
#include <cmath>
#include <cstdint>
#include <vector>

namespace r3d {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float k) const { return {x * k, y * k, z * k}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    float dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(Vec3 o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    float len() const { return std::sqrt(dot(*this)); }
    Vec3 norm() const { float l = len(); return l > 1e-6f ? *this * (1.f / l) : Vec3(0, 1, 0); }
};

// Матрица 4×4 по столбцам (как в GLSL): m[col * 4 + row].
struct Mat4 {
    float m[16];
    static Mat4 identity();
    static Mat4 translate(Vec3 t);
    static Mat4 scale(Vec3 s);
    static Mat4 rotY(float a);
    static Mat4 rotX(float a);
    static Mat4 rotZ(float a);
    static Mat4 perspective(float fovY, float aspect, float zn, float zf);
    static Mat4 lookAt(Vec3 eye, Vec3 at, Vec3 up);
    Mat4 operator*(const Mat4& o) const;
    Vec3 point(Vec3 p) const;   // преобразовать точку
    Vec3 dir(Vec3 d) const;     // преобразовать направление (без переноса)
};

inline Mat4 TRS(Vec3 t, float yaw, Vec3 s = {1, 1, 1}) { return Mat4::translate(t) * Mat4::rotY(yaw) * Mat4::scale(s); }

struct Vtx {
    float x, y, z;
    int8_t nx, ny, nz, pad;
    uint32_t color;
};

struct Mesh {
    uint32_t vbuf = 0, ibuf = 0;
    int count = 0;
    Vec3 bmin, bmax;
};

// Сборщик сетки из тел. Каждое тело можно повернуть/сдвинуть матрицей xf. Цвет — RGBA (gfx::rgba).
class Builder {
public:
    void box(Vec3 center, Vec3 half, uint32_t color, const Mat4& xf = Mat4::identity());
    // Цилиндр/усечённый конус по оси Y: основание в base, высота h, радиусы r0 (низ) и r1 (верх).
    void cylinder(Vec3 base, float r0, float r1, float h, uint32_t color, int segs = 12, const Mat4& xf = Mat4::identity(),
                  bool flat = false);
    void cone(Vec3 base, float r, float h, uint32_t color, int segs = 12, const Mat4& xf = Mat4::identity()) {
        cylinder(base, r, 0.f, h, color, segs, xf);
    }
    void sphere(Vec3 center, Vec3 radius, uint32_t color, int segs = 12, int rings = 8, const Mat4& xf = Mat4::identity());
    // Клин: брусок со скошенным верхом (скаты крыш). half — полуразмеры, верх сходится к ребру вдоль X.
    void wedge(Vec3 center, Vec3 half, uint32_t color, const Mat4& xf = Mat4::identity());
    void tri(Vec3 a, Vec3 b, Vec3 c, uint32_t color);
    void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t color);
    Mesh* build();  // создать буферы на GPU; сборщик очищается
    bool empty() const { return idx.empty(); }
    size_t vertexCount() const { return verts.size(); }

    std::vector<Vtx> verts;
    std::vector<uint32_t> idx;
};

struct PointLight {
    Vec3 pos;
    float radius = 0;      // 0 — выключен
    Vec3 color{1, 0.6f, 0.25f};
    float intensity = 1;
};

struct Lighting {
    Vec3 sunDir{-0.4f, -1.0f, -0.3f};  // куда светит солнце
    Vec3 sunColor{1.0f, 0.95f, 0.85f};
    Vec3 skyColor{0.55f, 0.62f, 0.75f};
    Vec3 groundColor{0.32f, 0.28f, 0.22f};
    Vec3 fogColor{0.70f, 0.78f, 0.86f};
    float fogNear = 30, fogFar = 90;
    PointLight lights[4];
};

struct Camera {
    Vec3 eye{0, 10, -10}, target{0, 0, 0};
    float fovY = 0.9f, zNear = 0.3f, zFar = 220.f;
};

bool init();
// Начать кадр: камера, свет, размер кадра в пикселях (для пропорций).
void beginFrame(const Camera& cam, const Lighting& light, float width, float height);
// tint умножает цвет вершин; emissive 0..1 — светится сам (огонь), не темнеет ночью.
void draw(const Mesh* mesh, const Mat4& model, uint32_t tint = 0xffffffffu, float emissive = 0.f);
// Отрисовать накопленное (вызывается из gfx::endFrame внутри прохода, до 2D).
void flush();
// Экранные координаты точки мира (в логических точках кадра); false — точка за камерой.
bool project(Vec3 p, float& sx, float& sy);
// Луч из точки экрана в мир: пересечение с плоскостью y = h.
bool unprojectToGround(float sx, float sy, float h, Vec3& out);
const Mat4& viewProj();
const Camera& camera();

}  // namespace r3d
