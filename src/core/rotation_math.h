#pragma once

#include <cmath>

namespace DL2HT {

// Minimum angle threshold for rotation (avoids unnecessary computation)
static constexpr float ROTATION_THRESHOLD = 0.001f;

// 3D vector type for clarity
struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3(const float* arr) : x(arr[0]), y(arr[1]), z(arr[2]) {}

    void ToArray(float* arr) const { arr[0] = x; arr[1] = y; arr[2] = z; }

    float Length() const { return sqrtf(x*x + y*y + z*z); }

    Vec3 Normalized() const {
        float len = Length();
        if (len < 0.0001f) return *this;
        return Vec3(x/len, y/len, z/len);
    }

    float Dot(const Vec3& other) const {
        return x*other.x + y*other.y + z*other.z;
    }

    Vec3 Cross(const Vec3& other) const {
        return Vec3(
            y*other.z - z*other.y,
            z*other.x - x*other.z,
            x*other.y - y*other.x
        );
    }
};

// Rotate vector around Y axis (yaw)
// DL2 coordinate system: X=forward, Y=up, Z=left
inline void RotateAroundY(Vec3& v, float angle) {
    if (fabsf(angle) < ROTATION_THRESHOLD) return;
    float c = cosf(angle), s = sinf(angle);
    float vx = v.x, vz = v.z;
    v.x = vx * c + vz * s;
    v.z = -vx * s + vz * c;
}

// Rodrigues rotation: rotate vector v around normalized axis by angle
// Formula: v_rot = v*cos(a) + (axis x v)*sin(a) + axis*(axis . v)*(1-cos(a))
inline void RotateAroundAxis(Vec3& v, const Vec3& axis, float angle) {
    if (fabsf(angle) < ROTATION_THRESHOLD) return;

    float c = cosf(angle), s = sinf(angle);
    Vec3 a = axis.Normalized();

    float dot = a.Dot(v);
    Vec3 cross = a.Cross(v);

    v.x = v.x*c + cross.x*s + a.x*dot*(1-c);
    v.y = v.y*c + cross.y*s + a.y*dot*(1-c);
    v.z = v.z*c + cross.z*s + a.z*dot*(1-c);
}

// Rodrigues rotation with pre-normalized axis and precomputed trig.
// Caller is responsible for ensuring unitAxis is unit-length.
inline void RotateAroundUnitAxis(Vec3& v, const Vec3& unitAxis, float cosAngle, float sinAngle) {
    float dot = unitAxis.Dot(v);
    Vec3 cross = unitAxis.Cross(v);
    float omc = 1.0f - cosAngle;

    v.x = v.x*cosAngle + cross.x*sinAngle + unitAxis.x*dot*omc;
    v.y = v.y*cosAngle + cross.y*sinAngle + unitAxis.y*dot*omc;
    v.z = v.z*cosAngle + cross.z*sinAngle + unitAxis.z*dot*omc;
}

// Apply yaw/pitch/roll head tracking rotations with horizon-locked yaw.
// Yaw rotates around world Y (vertical) so turning left/right stays on the
// horizon regardless of camera pitch. Pitch rotates around the camera's
// right vector. Up is re-derived to avoid coupling artifacts.
// DL2 coordinate system: X=forward, Y=up, Z=left
inline void ApplyHeadTrackingRotation(float* fwdArr, float* upArr, float yaw, float pitch, float roll) {
    Vec3 fwd(fwdArr);
    Vec3 up(upArr);

    static const Vec3 worldUp(0.0f, 1.0f, 0.0f);

    // Yaw: rotate around world Y axis (horizon-locked)
    if (fabsf(yaw) >= ROTATION_THRESHOLD) {
        float cy = cosf(yaw), sy = sinf(yaw);
        RotateAroundUnitAxis(fwd, worldUp, cy, sy);
        RotateAroundUnitAxis(up, worldUp, cy, sy);
    }

    // Pitch: rotate forward around camera's right vector
    if (fabsf(pitch) >= ROTATION_THRESHOLD) {
        Vec3 right = up.Cross(fwd).Normalized();
        float cp = cosf(pitch), sp = sinf(pitch);
        RotateAroundUnitAxis(fwd, right, cp, sp);
    }

    // Re-derive up perpendicular to new forward
    float dot = fwd.Dot(up);
    up = Vec3(up.x - fwd.x * dot, up.y - fwd.y * dot, up.z - fwd.z * dot).Normalized();

    // Roll: rotate up around forward axis
    if (fabsf(roll) >= ROTATION_THRESHOLD) {
        float cr = cosf(roll), sr = sinf(roll);
        RotateAroundUnitAxis(up, fwd, cr, sr);
    }

    fwd.ToArray(fwdArr);
    up.ToArray(upArr);
}

} // namespace DL2HT
