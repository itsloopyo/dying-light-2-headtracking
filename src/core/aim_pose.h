#pragma once

#include <cameraunlock/ads/ads_fade.h>
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
// eases out on core's AdsFade while the sights are up in sights locked; the game has no rig this
// mod can carry it on.
//
// The game moves more than one camera through the hooked call, and only the view camera's update
// reads the sights. So only that update advances the fade, and every camera takes the lean at the
// fade it left: fed "not aiming" from another camera, the fade would turn back every frame.
class AimPose {
public:
    // Once per update of the view camera, with this frame's sights.
    void Update(bool aiming, bool trueFreeLook, unsigned long long nowMs) {
        m_leanScale = m_fade.Update(aiming && !trueFreeLook, nowMs);
    }

    HeadPose Apply(const HeadPose& tracked, float zoomFactor) const {
        HeadPose out = tracked;
        out.yaw = cameraunlock::camera::ScaleAngleForZoom(tracked.yaw, zoomFactor);
        out.pitch = cameraunlock::camera::ScaleAngleForZoom(tracked.pitch, zoomFactor);
        out.lean = tracked.lean * (zoomFactor * m_leanScale);
        return out;
    }

    // Every frame the camera gets no head tracking: the next aim starts from the hip.
    void Stop() {
        m_fade.Reset();
        m_leanScale = 1.0f;
    }

private:
    cameraunlock::ads::AdsFade m_fade;
    float m_leanScale = 1.0f;
};

} // namespace DL2HT
