#pragma once

#include <cmath>

namespace DL2HT {

// Minimum angle threshold for rotation (avoids unnecessary computation)
static constexpr float ROTATION_THRESHOLD = 0.001f;

// Yaw rotation reference frame.
// CameraLocal: yaw around the camera's own up axis (default). When pitched,
//   yawing sweeps a cone around head-up and tilts the horizon.
// WorldLocked: yaw around world-up (DL2 Y axis). The horizon stays level;
//   looking down and yawing sweeps horizontally around world vertical.
enum class YawMode { CameraLocal, WorldLocked };

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

// Unit quaternion for singularity-free rotation composition.
// Avoids gimbal lock and Gram-Schmidt degeneration that occur with
// sequential Euler application at large combined pitch+roll angles.
struct Quat {
    float w, x, y, z;

    Quat() : w(1), x(0), y(0), z(0) {}
    Quat(float w_, float x_, float y_, float z_) : w(w_), x(x_), y(y_), z(z_) {}

    // Construct from rotation around a unit-length axis by angle (radians)
    static Quat FromAxisAngle(const Vec3& axis, float angle) {
        float half = angle * 0.5f;
        float s = sinf(half);
        return Quat(cosf(half), axis.x * s, axis.y * s, axis.z * s);
    }

    Quat operator*(const Quat& b) const {
        return Quat(
            w*b.w - x*b.x - y*b.y - z*b.z,
            w*b.x + x*b.w + y*b.z - z*b.y,
            w*b.y - x*b.z + y*b.w + z*b.x,
            w*b.z + x*b.y - y*b.x + z*b.w
        );
    }

    // Shortest-arc rotation from unit vector 'from' to unit vector 'to'
    static Quat ShortestArc(const Vec3& from, const Vec3& to) {
        float d = from.Dot(to);
        if (d >= 0.999999f) return Quat();
        if (d <= -0.999999f) {
            // 180° — use arbitrary perpendicular axis
            Vec3 perp = (fabsf(from.x) < 0.9f) ? Vec3(1,0,0) : Vec3(0,1,0);
            Vec3 axis = from.Cross(perp).Normalized();
            return Quat(0, axis.x, axis.y, axis.z);
        }
        Vec3 c = from.Cross(to);
        return Quat(1.0f + d, c.x, c.y, c.z).Normalized();
    }

    Quat Normalized() const {
        float len = sqrtf(w*w + x*x + y*y + z*z);
        if (len < 0.0001f) return Quat();
        float inv = 1.0f / len;
        return Quat(w*inv, x*inv, y*inv, z*inv);
    }

    // Rotate vector by this quaternion (assumes unit quaternion).
    // Optimized formula: v' = v + 2w(q×v) + 2(q×(q×v))
    Vec3 Rotate(const Vec3& v) const {
        float tx = 2.0f * (y*v.z - z*v.y);
        float ty = 2.0f * (z*v.x - x*v.z);
        float tz = 2.0f * (x*v.y - y*v.x);
        return Vec3(
            v.x + w*tx + (y*tz - z*ty),
            v.y + w*ty + (z*tx - x*tz),
            v.z + w*tz + (x*ty - y*tx)
        );
    }
};

// Apply yaw/pitch/roll head tracking rotations using spherical direction
// targeting with quaternion composition.
//
// Target forward is computed from spherical coordinates in the camera's local
// frame, so yaw produces pure screen-horizontal motion and pitch produces
// pure screen-vertical motion at any camera orientation — no "sticky spots"
// during compound rotations.
//
// A shortest-arc quaternion rotates the camera frame from original to target,
// avoiding Gram-Schmidt re-derivation and its singularity at extreme pitch.
// Roll is composed on top as rotation around the original forward axis.
//
// DL2 coordinate system: X=forward, Y=up, Z=left
inline void ApplyHeadTrackingRotation(float* fwdArr, float* upArr, float yaw, float pitch, float roll,
                                      YawMode mode = YawMode::CameraLocal) {
    Vec3 fwd(fwdArr);
    Vec3 up(upArr);

    if (fabsf(yaw) < ROTATION_THRESHOLD &&
        fabsf(pitch) < ROTATION_THRESHOLD &&
        fabsf(roll) < ROTATION_THRESHOLD)
        return;

    if (mode == YawMode::WorldLocked) {
        // M'' = R_pitchroll * M * R_yaw, mirroring resident-evil-requiem's
        // camera_hook.cpp. Yaw around world-up is pre-applied to every basis
        // vector (keeps horizon level). Pitch and roll then compose in the
        // *post-yaw local frame*: pitch around the rebuilt right-axis (which
        // stays horizontal when the game camera isn't rolling), roll around
        // the post-yaw forward. An earlier version derived pitch from
        // (cosP*fwdY - sinP*upY), which tilted the pitch plane by whatever
        // pitch the game had already baked into `up` and broke looking up/down.
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);

        Quat yawQ = Quat::FromAxisAngle(worldUp, yaw);
        Vec3 fwdY = yawQ.Rotate(fwd);
        Vec3 upY = yawQ.Rotate(up);
        Vec3 rightY = upY.Cross(fwdY).Normalized();

        // qPR = pitchQ * rollQ: roll is applied first (around post-yaw fwd),
        // then pitch (around post-yaw right). Matches RE:Requiem's qx * qz
        // intrinsic-YXZ composition.
        Quat pitchQ = Quat::FromAxisAngle(rightY, pitch);
        Quat rollQ = Quat::FromAxisAngle(fwdY, roll);
        Quat prQ = pitchQ * rollQ;

        fwd = prQ.Rotate(fwdY);
        up = prQ.Rotate(upY);

        fwd.ToArray(fwdArr);
        up.ToArray(upArr);
        return;
    }

    // Camera-local: spherical target in the camera's own frame, then
    // shortest-arc to align + roll around original fwd.
    Vec3 right = up.Cross(fwd).Normalized();
    float cosY = cosf(yaw), sinY = sinf(yaw);
    float cosP = cosf(pitch), sinP = sinf(pitch);

    Vec3 targetFwd = Vec3(
        cosP*cosY*fwd.x + cosP*sinY*right.x - sinP*up.x,
        cosP*cosY*fwd.y + cosP*sinY*right.y - sinP*up.y,
        cosP*cosY*fwd.z + cosP*sinY*right.z - sinP*up.z
    ).Normalized();

    Quat q = Quat::ShortestArc(fwd, targetFwd)
           * Quat::FromAxisAngle(fwd, roll);

    fwd = q.Rotate(fwd);
    up = q.Rotate(up);

    fwd.ToArray(fwdArr);
    up.ToArray(upArr);
}

} // namespace DL2HT
