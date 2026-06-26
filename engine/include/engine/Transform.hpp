#pragma once
#include "engine/Math.hpp"

namespace engine {

// Rotazione in gradi (Euler XYZ), più intuitiva da editare nell'Inspector
// rispetto ai radianti o ai quaternioni.
struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 rotationDegrees{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    Mat4 getMatrix() const {
        Mat4 t = Mat4::translate(position);
        Mat4 rx = Mat4::rotateX(radians(rotationDegrees.x));
        Mat4 ry = Mat4::rotateY(radians(rotationDegrees.y));
        Mat4 rz = Mat4::rotateZ(radians(rotationDegrees.z));
        Mat4 s = Mat4::scale(scale);
        return t * rz * ry * rx * s;
    }
};

} // namespace engine
