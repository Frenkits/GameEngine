#pragma once
#include "engine/Math.hpp"

namespace engine {

// Camera "orbit" tipica degli editor 3D (Unity/Blender style):
// ruota intorno a un punto target, lo zoom avvicina/allontana la distanza.
class OrbitCamera {
public:
    Vec3 target{0.0f, 0.0f, 0.0f};
    float distance = 8.0f;
    float yawDegrees = 45.0f;     // rotazione orizzontale
    float pitchDegrees = 30.0f;   // rotazione verticale
    float fovDegrees = 60.0f;
    float nearPlane = 0.05f;
    float farPlane = 500.0f;

    // Chiamala quando l'utente trascina il mouse col tasto destro premuto
    void orbit(float deltaX, float deltaY) {
        const float sensitivity = 0.25f;
        yawDegrees -= deltaX * sensitivity;
        pitchDegrees -= deltaY * sensitivity;
        // Evita che la camera si "ribalti" superando i poli
        if (pitchDegrees > 89.0f) pitchDegrees = 89.0f;
        if (pitchDegrees < -89.0f) pitchDegrees = -89.0f;
    }

    // Chiamala con il delta della rotella del mouse
    void zoom(float scrollDelta) {
        distance -= scrollDelta * (distance * 0.1f + 0.1f);
        if (distance < 0.5f) distance = 0.5f;
        if (distance > 200.0f) distance = 200.0f;
    }

    // Pan (trascinamento con tasto centrale/shift+sinistro nei vari editor)
    void pan(float deltaX, float deltaY) {
        Vec3 eye = getEyePosition();
        Vec3 forward = (target - eye).normalized();
        Vec3 right = Vec3::cross(forward, {0, 1, 0}).normalized();
        Vec3 up = Vec3::cross(right, forward);
        const float panSpeed = distance * 0.002f;
        target = target - right * (deltaX * panSpeed) + up * (deltaY * panSpeed);
    }

    Vec3 getEyePosition() const {
        float yaw = radians(yawDegrees);
        float pitch = radians(pitchDegrees);
        Vec3 offset{
            distance * std::cos(pitch) * std::sin(yaw),
            distance * std::sin(pitch),
            distance * std::cos(pitch) * std::cos(yaw)
        };
        return target + offset;
    }

    Mat4 getViewMatrix() const {
        return Mat4::lookAt(getEyePosition(), target, Vec3{0, 1, 0});
    }

    Mat4 getProjectionMatrix(float aspectRatio) const {
        return Mat4::perspective(radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
};

} // namespace engine
