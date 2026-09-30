#pragma once

#include <cameraunlock/rendering/aim_ndc_projection.h>
#include <cameraunlock/math/vec3.h>

namespace DL2HT {

inline bool ProjectAim(const cameraunlock::math::Vec3& toAim, const float forward[3],
                       const float up[3], float& tanRight, float& tanUp) {
    const auto direction = toAim.Normalized();
    const float aim[3] = {direction.x, direction.y, direction.z};
    // FromForwardUpPos takes the camera's backward axis, as FromLookAt's eye - target does.
    const float viewForward[3] = {-forward[0], -forward[1], -forward[2]};
    const float screenRight[3] = {up[1] * forward[2] - up[2] * forward[1],
                                  up[2] * forward[0] - up[0] * forward[2],
                                  up[0] * forward[1] - up[1] * forward[0]};
    return cameraunlock::rendering::ProjectAimToNdc(aim, viewForward, screenRight, up,
                                                   1.0f, 1.0f, tanRight, tanUp);
}

inline bool AimScreenPosition(float tanRight, float tanUp, float verticalFov,
                              float width, float height, float& x, float& y) {
    if (!(verticalFov > 0.0f && verticalFov < 180.0f) || !(width > 0.0f && height > 0.0f)) return false;
    constexpr float halfDegToRad = 3.14159265358979323846f / 360.0f;
    const float focalLength = height * 0.5f / std::tan(verticalFov * halfDegToRad);
    x = width * 0.5f + tanRight * focalLength;
    y = height * 0.5f - tanUp * focalLength;
    return std::isfinite(x) && std::isfinite(y) && x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

} // namespace DL2HT
