#include "pch.h"
#include "lean_trace.h"
#include "core/logger.h"

#include <cameraunlock/memory/safe_memory.h>

#include <cmath>
#include <cstdint>
#include <cstring>

// IGSObject::Raytrace reads [this+0x20]->+0x48 (the level) and calls the level's virtual line cast
// with a collision info it builds from the arguments. It moves `to` back to the hit point and
// returns non-zero on a hit. SCollision begins with the surface normal.
//
// The arguments below are those the game's own gameplay queries pass (type 7: the physics world
// and the terrain), with the filter mask 0x100e1e00 that several of them use. With mask 0 the
// cast hits the player's own body a few centimetres from the eye; with this one it does not.

namespace DL2HT::lean_trace {

namespace {

namespace mem = cameraunlock::memory;
using cameraunlock::math::Vec3;

struct EntitySpan {
    const void* data;
    uint64_t count;
};

using RaytraceFn = uint8_t (*)(void* self, uint8_t type, void* collision, const float* from, float* to, uint16_t a6,
                               bool a7, void* ignore, uint32_t flags, uint16_t a10, int64_t mask,
                               const EntitySpan* entities);

constexpr uint8_t kRaytraceType = 7;
constexpr int64_t kCollisionMask = 0x100e1e00;
// Bullet's primary line query excludes this flag and passes the shooter's IControlObject.
constexpr int64_t kBulletCollisionMask = 0x04000000;
// Barrier collision hulls can extend above visible railings and must not set the aim depth.
constexpr int64_t kBarrierCollisionGroup = 0x20;
constexpr int kCameraEngineOffset = 0x20;
constexpr int kEngineLevelOffset = 0x48;
constexpr int kLevelDiLevelOffset = 0x38;

// How far off the centre ray the normal probes run, and how far apart their distances may be for
// the three hits to count as one surface: a plane 75 degrees off the ray moves the hit by
// kNormalProbe * tan(75) = 0.075 over that offset.
constexpr float kNormalProbe = 0.02f;
constexpr float kSameSurface = 0.08f;

RaytraceFn g_raytrace = nullptr;
int g_nearClipOffset = -1;
void* g_validatedCamera = nullptr;
void* g_validatedLevelDI = nullptr;
bool g_cameraValid = false;
unsigned g_casts = 0;
int64_t g_ticks = 0;

struct RawHit {
    bool hit = false;
    float distance = 0.0f;
    Vec3 point;
    uint64_t component = 0;
    uint64_t entity = 0;
};

RawHit Raw(void* camera, const Vec3& start, const Vec3& direction, float length,
           int64_t mask = kCollisionMask, void* ignore = nullptr) {
    const float from[4] = {start.x, start.y, start.z, 0.0f};
    float to[4] = {start.x + direction.x * length, start.y + direction.y * length, start.z + direction.z * length,
                   0.0f};
    alignas(16) uint8_t collision[0x40] = {};
    const EntitySpan none{nullptr, 0};

    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    const uint8_t hit =
        g_raytrace(camera, kRaytraceType, collision, from, to, 0, false, ignore, 0, 0, mask, &none);
    QueryPerformanceCounter(&t1);
    ++g_casts;
    g_ticks += t1.QuadPart - t0.QuadPart;

    RawHit out;
    out.hit = hit != 0;
    out.point = Vec3(to[0], to[1], to[2]);
    if (out.hit) {
        const float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
        out.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        std::memcpy(&out.component, collision + 0x28, sizeof(out.component));
        std::memcpy(&out.entity, collision + 0x30, sizeof(out.entity));
    }
    return out;
}

// The normal of the surface a centre-ray hit landed on, from two hits a little to either side.
// Zero when either side ray missed or found another surface, which core reads as the steepest
// angle it allows.
Vec3 MeasureNormal(void* camera, const Vec3& start, const Vec3& direction, float length, float distance) {
    const float ax = std::fabs(direction.x), ay = std::fabs(direction.y), az = std::fabs(direction.z);
    const Vec3 helper = (ax <= ay && ax <= az) ? Vec3(1, 0, 0) : (ay <= az) ? Vec3(0, 1, 0) : Vec3(0, 0, 1);
    const Vec3 u = Vec3::Cross(direction, helper).Normalized();
    const Vec3 v = Vec3::Cross(direction, u);

    const RawHit hu = Raw(camera, start + u * kNormalProbe, direction, length);
    const RawHit hv = Raw(camera, start + v * kNormalProbe, direction, length);
    if (!hu.hit || !hv.hit) return Vec3::Zero();
    if (std::fabs(hu.distance - distance) > kSameSurface || std::fabs(hv.distance - distance) > kSameSurface) {
        return Vec3::Zero();
    }
    const Vec3 p0 = start + direction * distance;
    const Vec3 pu = start + u * kNormalProbe + direction * hu.distance;
    const Vec3 pv = start + v * kNormalProbe + direction * hv.distance;
    const Vec3 n = Vec3::Cross(pu - p0, pv - p0);
    const float len = n.Magnitude();
    if (!(len > 1e-8f)) return Vec3::Zero();
    return n * (1.0f / len);
}

}  // namespace

bool Initialize() {
    HMODULE engine = GetModuleHandleA("engine_x64_rwdi.dll");
    if (engine) {
        g_raytrace = reinterpret_cast<RaytraceFn>(GetProcAddress(
            engine, "?Raytrace@IGSObject@@QEAAEEPEAUSCollision@@AEBVvec3@@AEAV3@G_NPEAVIControlObject@@IG_JV?$span@"
                    "PEBVCEntity@cbs@@$0PPPPPPPP@@ttl@@@Z"));
    }
    if (!g_raytrace) {
        Logger::Instance().Warning("Lean collision: IGSObject::Raytrace is not exported, so leaning is not kept "
                                   "out of walls");
        return false;
    }
    Logger::Instance().Info("Lean collision: IGSObject::Raytrace at %p", g_raytrace);

    // IBaseCamera::GetClipNear: mov rax,[rcx+inner]; movss xmm0,[rax+near]; ret
    const auto* getNear = reinterpret_cast<const uint8_t*>(GetProcAddress(engine, "?GetClipNear@IBaseCamera@@QEBAMXZ"));
    if (getNear && getNear[0] == 0x48 && getNear[1] == 0x8B && getNear[2] == 0x41 && getNear[4] == 0xF3 &&
        getNear[5] == 0x0F && getNear[6] == 0x10 && getNear[7] == 0x80 && getNear[12] == 0xC3) {
        std::memcpy(&g_nearClipOffset, getNear + 8, sizeof(g_nearClipOffset));
    } else {
        Logger::Instance().Warning("Lean collision: IBaseCamera::GetClipNear does not read the near clip as expected");
    }
    return true;
}

bool NearClip(void* engineCamera, float& out) {
    return g_nearClipOffset >= 0 && engineCamera &&
           mem::SafeRead(reinterpret_cast<uintptr_t>(engineCamera) + g_nearClipOffset, out);
}

bool Validate(void* viewCamera, void* levelDI) {
    if (!g_raytrace || !viewCamera) return false;
    if (viewCamera == g_validatedCamera && levelDI == g_validatedLevelDI) return g_cameraValid;
    g_validatedCamera = viewCamera;
    g_validatedLevelDI = levelDI;

    void* engineCamera = nullptr;
    void* cameraLevel = nullptr;
    void* activeLevel = nullptr;
    g_cameraValid = mem::SafeRead(reinterpret_cast<uintptr_t>(viewCamera) + kCameraEngineOffset, engineCamera) &&
                    engineCamera &&
                    mem::SafeRead(reinterpret_cast<uintptr_t>(engineCamera) + kEngineLevelOffset, cameraLevel) &&
                    levelDI &&
                    mem::SafeRead(reinterpret_cast<uintptr_t>(levelDI) + kLevelDiLevelOffset, activeLevel) &&
                    cameraLevel && cameraLevel == activeLevel;
    if (g_cameraValid) {
        Logger::Instance().Info("Lean collision: view camera %p casts through level %p", viewCamera, cameraLevel);
    } else {
        Logger::Instance().Warning("Lean collision: view camera %p does not lead to the active level (%p, expected "
                                   "%p), so its lean is not clamped",
                                   viewCamera, cameraLevel, activeLevel);
    }
    return g_cameraValid;
}

cameraunlock::camera::LineHit Cast(void* context, const Vec3& start, const Vec3& direction, float length) {
    const Context& ctx = *static_cast<const Context*>(context);
    cameraunlock::camera::LineHit out;
    if (!g_raytrace || !g_cameraValid || ctx.viewCamera != g_validatedCamera) return out;

    const RawHit hit = Raw(ctx.viewCamera, start, direction, length);
    out.queried = true;
    out.hit = hit.hit;
    out.distance = hit.distance;
    const bool centre = (start - ctx.eye).SqrMagnitude() < 1e-10f && Vec3::Dot(direction, ctx.leanDirection) > 0.9999f;
    if (hit.hit && centre) out.normal = MeasureNormal(ctx.viewCamera, start, direction, length, hit.distance);
    return out;
}

bool AimPoint(void* viewCamera, void* player, const Vec3& eye, const Vec3& direction, float length,
              bool& hit, Vec3& point) {
    if (!g_raytrace || !g_cameraValid || viewCamera != g_validatedCamera || !player) return false;
    const RawHit h = Raw(viewCamera, eye, direction, length, kBulletCollisionMask | kBarrierCollisionGroup,
                         static_cast<uint8_t*>(player) + 0x10);
    hit = h.hit;
    point = h.point;
    static ULONGLONG sampleTick = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - sampleTick >= 1000) {
        sampleTick = now;
        Logger::Instance().Info("AIMTRACE eye=(%.4f,%.4f,%.4f) dir=(%.6f,%.6f,%.6f) "
                                "aim=(%d,%.4f,%llX,%llX) "
                                "point=(%.4f,%.4f,%.4f)",
                                eye.x, eye.y, eye.z, direction.x, direction.y, direction.z,
                                h.hit, h.distance, h.component, h.entity,
                                h.point.x, h.point.y, h.point.z);
    }
    return true;
}

void TakeStats(unsigned& casts, double& micros) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    casts = g_casts;
    micros = static_cast<double>(g_ticks) * 1e6 / static_cast<double>(freq.QuadPart);
    g_casts = 0;
    g_ticks = 0;
}

}  // namespace DL2HT::lean_trace
