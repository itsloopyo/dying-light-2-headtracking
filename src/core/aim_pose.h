#pragma once

#include <cameraunlock/ads/lean_handover.h>
#include <cameraunlock/camera/zoom_compensation.h>
#include <cameraunlock/math/vec3.h>

#include <cmath>

namespace DL2HT {

// The tracker pose in the mod's own units: degrees, and the lean in metres on the processor's axes.
struct HeadPose {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    cameraunlock::math::Vec3 lean;
};

// tan(live/2) / tan(base/2) for two vertical FOVs in degrees: 1 when nothing zooms, below 1 when
// the view is narrowed.
inline float ZoomFactor(float liveFovDeg, float baseFovDeg) {
    constexpr float kHalfDegToRad = 3.14159265358979323846f / 360.0f;
    return cameraunlock::camera::FovZoomFactor(std::tan(liveFovDeg * kHalfDegToRad),
                                               std::tan(baseFovDeg * kHalfDegToRad));
}

// Head tracking through the aim, sights locked unless true free look is on. Rotation is never
// faded: it only scales with the zoom, and roll not even then. The lean scales with the zoom and
// eases out while the sights are up in sights locked; the game has no rig this mod can carry it
// on, so the hand-over's rig share is always empty.
class AimPose {
public:
    HeadPose Apply(const HeadPose& tracked, bool aiming, bool trueFreeLook, float zoomFactor,
                   unsigned long long nowMs) {
        HeadPose out = tracked;
        out.yaw = cameraunlock::camera::ScaleAngleForZoom(tracked.yaw, zoomFactor);
        out.pitch = cameraunlock::camera::ScaleAngleForZoom(tracked.pitch, zoomFactor);
        const cameraunlock::ads::LeanShares shares =
            m_handover.Update(tracked.lean * zoomFactor, aiming, trueFreeLook, false, nowMs);
        out.lean = shares.camera;
        return out;
    }

    // Every frame the camera gets no head tracking: the next aim starts from the hip.
    void Stop() { m_handover.Stop(); }

private:
    cameraunlock::ads::LeanHandover m_handover;
};

} // namespace DL2HT
