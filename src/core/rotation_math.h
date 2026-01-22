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

// Apply yaw/pitch/roll head tracking rotations using spherical coordinates
// in the game camera's local frame. This constructs the final direction directly
// rather than sequential rotations, so yaw and pitch are fully independent —
// yawing while pitched doesn't produce arc artifacts.
// Up is re-derived to avoid coupling/circular artifacts.
// DL2 coordinate system: X=forward, Y=up, Z=left
inline void ApplyHeadTrackingRotation(float* fwdArr, float* upArr, float yaw, float pitch, float roll) {
    Vec3 origFwd(fwdArr);
    Vec3 origUp(upArr);
    Vec3 origRight = origUp.Cross(origFwd).Normalized();

    // Construct new forward from spherical coordinates in camera frame.
    // fwd = cos(pitch)*cos(yaw)*forward + cos(pitch)*sin(yaw)*right - sin(pitch)*up
    // This ensures yaw produces pure screen-horizontal motion at any pitch,
    // and pitch produces pure screen-vertical motion at any yaw.
    float cosY = cosf(yaw), sinY = sinf(yaw);
    float cosP = cosf(pitch), sinP = sinf(pitch);

    Vec3 fwd(
        cosP * cosY * origFwd.x + cosP * sinY * origRight.x - sinP * origUp.x,
        cosP * cosY * origFwd.y + cosP * sinY * origRight.y - sinP * origUp.y,
        cosP * cosY * origFwd.z + cosP * sinY * origRight.z - sinP * origUp.z
    );

    // Re-derive up by projecting original game up perpendicular to new forward.
    float dot = fwd.Dot(origUp);
    Vec3 up(origUp.x - fwd.x * dot, origUp.y - fwd.y * dot, origUp.z - fwd.z * dot);
    up = up.Normalized();

    // Apply head tracking roll around forward
    if (fabsf(roll) >= ROTATION_THRESHOLD) {
        float cr = cosf(roll), sr = sinf(roll);
        RotateAroundUnitAxis(up, fwd, cr, sr);
    }

    fwd.ToArray(fwdArr);
    up.ToArray(upArr);
}

} // namespace DL2HT
