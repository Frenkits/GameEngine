#pragma once
#include <cmath>
#include <array>

namespace engine {

constexpr float PI = 3.14159265358979323846f;
inline float radians(float degrees) { return degrees * PI / 180.0f; }

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }

    Vec3 normalized() const {
        float l = length();
        if (l < 1e-8f) return {0, 0, 0};
        return {x / l, y / l, z / l};
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

// Matrice 4x4 column-major, compatibile con il layout che OpenGL si aspetta
struct Mat4 {
    std::array<float, 16> m{};

    static Mat4 identity() {
        Mat4 r;
        r.m = {1,0,0,0,
               0,1,0,0,
               0,0,1,0,
               0,0,0,1};
        return r;
    }

    static Mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
        Mat4 r = identity();
        r.m[0]  = 2.0f / (right - left);
        r.m[5]  = 2.0f / (top - bottom);
        r.m[10] = -2.0f / (zFar - zNear);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(zFar + zNear) / (zFar - zNear);
        return r;
    }

    static Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
        Mat4 r{};
        float f = 1.0f / std::tan(fovYRadians * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = Vec3::cross(f, up).normalized();
        Vec3 u = Vec3::cross(s, f);

        Mat4 r = identity();
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -Vec3::dot(s, eye);
        r.m[13] = -Vec3::dot(u, eye);
        r.m[14] = Vec3::dot(f, eye);
        return r;
    }

    static Mat4 translate(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r = identity();
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }

    static Mat4 rotateX(float radians) {
        Mat4 r = identity();
        float c = std::cos(radians), s = std::sin(radians);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotateY(float radians) {
        Mat4 r = identity();
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotateZ(float radians) {
        Mat4 r = identity();
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c; r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    // Moltiplicazione column-major: (*this) * o, applicata come this->o->vettore
    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[k * 4 + row] * o.m[col * 4 + k];
                }
                r.m[col * 4 + row] = sum;
            }
        }
        return r;
    }

    const float* data() const { return m.data(); }
};

} // namespace engine
