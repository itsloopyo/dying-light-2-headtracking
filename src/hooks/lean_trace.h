#pragma once

#include <cameraunlock/camera/lean_line_sweep.h>
#include <cameraunlock/math/vec3.h>

namespace DL2HT::lean_trace {

// The engine's line cast for core's LineSweepQuery. The engine exposes no sweep with a radius, so
// the sphere around the eye is built from these by core.
struct Context {
    // The view camera (ILevel::GetViewCamera), which is the IGSObject the cast runs on.
    void* viewCamera = nullptr;
    // The lean LineSweepQuery is answering this frame. Only its centre ray, from `eye` along
    // `leanDirection`, pays for the two extra casts that measure the surface normal.
    cameraunlock::math::Vec3 eye;
    cameraunlock::math::Vec3 leanDirection;
};

// Resolves IGSObject::Raytrace from the engine's exports. False, with a log line, when the export
// is missing, which leaves the lean unclamped.
bool Initialize();

// The near clip distance of `engineCamera` (the camera MoveCameraFromForwardUpPos runs on), read
// where IBaseCamera::GetClipNear reads it. False when that accessor did not decode.
bool NearClip(void* engineCamera, float& out);

// Whether `viewCamera` reaches the level the way the cast relies on: its engine camera's level is
// the active level's. Checked once per view camera and level. A mismatch is logged and every cast
// through that camera reports queried=false.
bool Validate(void* viewCamera, void* levelDI);

// A LineCastFn. `context` is a Context.
cameraunlock::camera::LineHit Cast(void* context, const cameraunlock::math::Vec3& start,
                                   const cameraunlock::math::Vec3& direction, float length);

// How far the clean aim runs from `eye` along `direction` before something stops it, for the aim
// dot. False when the cast cannot run through `viewCamera` (see Validate); `hit` false with a true
// return is a definite clear line out to `length`.
bool AimDistance(void* viewCamera, const cameraunlock::math::Vec3& eye, const cameraunlock::math::Vec3& direction,
                 float length, bool& hit, float& distance);

// Casts since the last call, and the time they took in microseconds, for the periodic log line.
void TakeStats(unsigned& casts, double& micros);

} // namespace DL2HT::lean_trace
