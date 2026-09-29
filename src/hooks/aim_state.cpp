#include "pch.h"
#include "aim_state.h"
#include "core/logger.h"

#include <cameraunlock/memory/pattern_scanner.h>
#include <cameraunlock/memory/safe_memory.h>

#include <cmath>
#include <cstring>

// The first-person camera is a CameraFPPDI whose update asks the player's first-person visual
// (PlayerFppVis_PH, at a fixed offset in the camera) for the eye, the view basis and the FOV, then
// sets the FOV as base / zoom, where base is the camera's own smoothed FOV and zoom a divisor the
// visual reads from the player's firearm and bow modules (1 at the hip). The firearm module also
// holds whether its sights are up; the game's crosshair reads it through the one small accessor
// resolved below. Every offset and address here is read out of the loaded code, each pattern
// matched exactly once, so a patch that moves them leaves the feature off with a log line rather
// than reading the wrong field.

namespace DL2HT {

namespace {

namespace mem = cameraunlock::memory;

using GetViewCameraFn = void* (*)(void* level);
using GetComponentFn = void* (*)(void* vars, int id);
using IsIronsightAimingFn = bool (*)(void* firearmModule);

struct Resolved {
    // engine: IBaseCamera -> engine camera, and that camera's FOV in radians.
    int innerOffset = -1;
    int fovOffset = -1;
    GetViewCameraFn getViewCamera = nullptr;
    // gamedll: CameraFPPDI -> first-person visual, and CameraFPPDI's un-zoomed FOV in degrees.
    int visualOffset = -1;
    int baseFovOffset = -1;
    bool fovReady = false;
    // gamedll: the visual's zoom accessor, which names the firearm module.
    int zoomSlot = -1;
    void* zoomFn = nullptr;
    const int* firearmIdSlot = nullptr;
    int visualVarsOffset = 0;
    GetComponentFn getComponent = nullptr;
    IsIronsightAimingFn isIronsightAiming = nullptr;
    bool aimReady = false;
};

Resolved g;
bool g_readFaultLogged = false;

void ReadFault(const char* what) {
    if (g_readFaultLogged) return;
    g_readFaultLogged = true;
    Logger::Instance().Warning("Aim state: could not read %s; the sights read as down this frame", what);
}

bool ModuleRange(const char* name, uintptr_t& base, size_t& size) {
    HMODULE module = GetModuleHandleA(name);
    return module && mem::GetModuleRange(module, base, size);
}

// The one match of `pattern` in the module, or null when there is none or more than one.
uint8_t* UniqueMatch(const char* moduleName, const char* pattern, const char* what) {
    uintptr_t base = 0;
    size_t size = 0;
    if (!ModuleRange(moduleName, base, size)) {
        Logger::Instance().Warning("Aim state: %s not loaded, so %s is unavailable", moduleName, what);
        return nullptr;
    }
    auto* first = static_cast<uint8_t*>(mem::ScanPatternInRange(base, size, pattern));
    if (!first) {
        Logger::Instance().Warning("Aim state: %s not found in %s", what, moduleName);
        return nullptr;
    }
    const uintptr_t next = reinterpret_cast<uintptr_t>(first) + 1;
    if (mem::ScanPatternInRange(next, base + size - next, pattern)) {
        Logger::Instance().Warning("Aim state: %s matches more than once in %s", what, moduleName);
        return nullptr;
    }
    return first;
}

int32_t Rel32(const uint8_t* at) {
    int32_t value;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

bool ResolveEngine() {
    HMODULE engine = GetModuleHandleA("engine_x64_rwdi.dll");
    if (!engine) return false;
    // IBaseCamera::GetFOV: mov rax,[rcx+inner]; movss xmm0,[rax+fov]; mulss xmm0,[rad->deg]; ret
    const auto* getFov = reinterpret_cast<const uint8_t*>(GetProcAddress(engine, "?GetFOV@IBaseCamera@@QEBAMXZ"));
    if (!getFov || getFov[0] != 0x48 || getFov[1] != 0x8B || getFov[2] != 0x41 || getFov[4] != 0xF3 ||
        getFov[5] != 0x0F || getFov[6] != 0x10 || getFov[7] != 0x80 || getFov[12] != 0xF3 || getFov[13] != 0x0F ||
        getFov[14] != 0x59) {
        Logger::Instance().Warning("Aim state: IBaseCamera::GetFOV does not read the camera FOV as expected");
        return false;
    }
    float toDegrees = 0.0f;
    std::memcpy(&toDegrees, getFov + 20 + Rel32(getFov + 16), sizeof(toDegrees));
    if (std::fabs(toDegrees - 57.2957795f) > 1e-3f) {
        Logger::Instance().Warning("Aim state: IBaseCamera::GetFOV scales by %f, not radians to degrees", toDegrees);
        return false;
    }
    g.innerOffset = getFov[3];
    g.fovOffset = Rel32(getFov + 8);
    g.getViewCamera =
        reinterpret_cast<GetViewCameraFn>(GetProcAddress(engine, "?GetViewCamera@ILevel@@QEBAPEAVIBaseCamera@@XZ"));
    if (!g.getViewCamera) {
        Logger::Instance().Warning("Aim state: ILevel::GetViewCamera not exported");
        return false;
    }
    return true;
}

bool ResolveFov() {
    // CameraFPPDI's update: SetFOV(visual->Override()) when the visual overrides the FOV, else
    // SetFOV(base / zoom). The visual's offset and base's are the two displacements.
    uint8_t* site = UniqueMatch("gamedll_ph_x64_rwdi.dll",
                                "84 C0 74 ?? 48 8B 4F ?? 48 8B 01 FF 50 ?? 0F 28 C8 EB ?? F3 0F 10 8F ?? ?? ?? ?? "
                                "F3 41 0F 5E CA 48 8B CF FF 15",
                                "the first-person camera's FOV update");
    if (!site) return false;
    HMODULE engine = GetModuleHandleA("engine_x64_rwdi.dll");
    void* setFov = engine ? reinterpret_cast<void*>(GetProcAddress(engine, "?SetFOV@IBaseCamera@@QEAAXM@Z")) : nullptr;
    void* called = nullptr;
    if (!setFov || !mem::SafeRead(reinterpret_cast<uintptr_t>(site + 41 + Rel32(site + 37)), called) ||
        called != setFov) {
        Logger::Instance().Warning("Aim state: the first-person camera's FOV update does not call IBaseCamera::SetFOV");
        return false;
    }
    g.visualOffset = site[7];
    g.baseFovOffset = Rel32(site + 23);
    return true;
}

bool ResolveAim() {
    // Earlier in the same update: rcx = camera->visual; call [vtable + slot]; the zoom divisor.
    uint8_t* zoomCall = UniqueMatch("gamedll_ph_x64_rwdi.dll", "48 8B 4F ?? 48 8B 01 FF 50 ?? 48 8B 4F ?? 44 0F 28 D0",
                                    "the first-person camera's zoom call");
    // The visual's zoom accessor. It opens by fetching the player's firearm module:
    // mov edx,[firearm module id]; mov rcx,[this+vars]; ...; call GetComponent; ...
    uint8_t* zoom = UniqueMatch("gamedll_ph_x64_rwdi.dll",
                                "40 53 48 83 EC 30 8B 15 ?? ?? ?? ?? 48 8B D9 48 8B 89 ?? ?? ?? ?? 0F 29 74 24 20 83 FA "
                                "FF 75 16 8B 05 ?? ?? ?? ?? 8B D0 89 05 ?? ?? ?? ?? FF C0 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? "
                                "48 8B C8 E8",
                                "the first-person visual's zoom accessor");
    // The firearm module's sights test, which the crosshair reads: [module+up] || debug flag.
    uint8_t* sights = UniqueMatch("gamedll_ph_x64_rwdi.dll",
                                  "80 B9 ?? ?? ?? ?? 00 75 0C 80 3D ?? ?? ?? ?? 00 75 03 32 C0 C3 B0 01 C3",
                                  "the firearm sights test");
    if (!zoomCall || !zoom || !sights) return false;
    if (zoomCall[3] != static_cast<uint8_t>(g.visualOffset) || zoomCall[13] != static_cast<uint8_t>(g.visualOffset)) {
        Logger::Instance().Warning("Aim state: the zoom call reads the visual at 0x%X, the FOV update at 0x%X",
                                   zoomCall[3], g.visualOffset);
        return false;
    }
    g.zoomSlot = zoomCall[9] / 8;
    g.zoomFn = zoom;
    g.firearmIdSlot = reinterpret_cast<const int*>(zoom + 12 + Rel32(zoom + 8));
    g.visualVarsOffset = Rel32(zoom + 18);
    g.getComponent = reinterpret_cast<GetComponentFn>(zoom + 59 + Rel32(zoom + 55));
    g.isIronsightAiming = reinterpret_cast<IsIronsightAimingFn>(sights);
    return true;
}

bool FloatAt(void* base, int offset, float& out) {
    return mem::SafeRead(reinterpret_cast<uintptr_t>(base) + offset, out);
}

bool PtrAt(void* base, int offset, void*& out) {
    return mem::SafeRead(reinterpret_cast<uintptr_t>(base) + offset, out);
}

bool UsableFov(float deg) { return std::isfinite(deg) && deg > 0.0f && deg < 180.0f; }

}  // namespace

void InitializeAimState() {
    if (!ResolveEngine()) return;
    g.fovReady = ResolveFov();
    g.aimReady = g.fovReady && ResolveAim();
    Logger::Instance().Info("Aim state: FOV terms %s (camera->engine +0x%X, fov +0x%X, visual +0x%X, base +0x%X); "
                            "sights %s (zoom slot %d, vars %d)",
                            g.fovReady ? "resolved" : "unavailable", g.innerOffset, g.fovOffset, g.visualOffset,
                            g.baseFovOffset, g.aimReady ? "resolved" : "unavailable", g.zoomSlot, g.visualVarsOffset);
}

FppCameraSample SampleFppCamera(void* innerCamera, void* level) {
    FppCameraSample s;
    if (!g.getViewCamera || !level || !innerCamera) return s;
    void* camera = g.getViewCamera(level);
    void* engineCamera = nullptr;
    if (!camera || !PtrAt(camera, g.innerOffset, engineCamera)) return s;
    if (engineCamera != innerCamera) return s;
    s.view = true;

    float liveRad = 0.0f;
    if (FloatAt(innerCamera, g.fovOffset, liveRad) && UsableFov(liveRad * 57.2957795f)) {
        s.liveFovKnown = true;
        s.liveFovDeg = liveRad * 57.2957795f;
    }

    float baseDeg = 0.0f;
    void* visual = nullptr;
    if (!g.fovReady || !PtrAt(camera, g.visualOffset, visual) || !visual) return s;
    // Only the player's first-person visual has this zoom accessor in that vtable slot, so another
    // camera's controller is never taken for it.
    void** vtable = nullptr;
    void* slot = nullptr;
    if (!PtrAt(visual, 0, reinterpret_cast<void*&>(vtable)) || !vtable ||
        !mem::SafeRead(reinterpret_cast<uintptr_t>(vtable + g.zoomSlot), slot)) {
        ReadFault("the first-person visual's vtable");
        return s;
    }
    if (g.aimReady && slot != g.zoomFn) return s;
    s.fpp = true;

    if (s.liveFovKnown && FloatAt(camera, g.baseFovOffset, baseDeg) && UsableFov(baseDeg)) {
        s.fovKnown = true;
        s.baseFovDeg = baseDeg;
    }

    if (!g.aimReady) return s;
    int id = -1;
    void* vars = nullptr;
    if (!mem::SafeRead(reinterpret_cast<uintptr_t>(g.firearmIdSlot), id) ||
        !PtrAt(visual, g.visualVarsOffset, vars)) {
        ReadFault("the firearm module's owner");
        return s;
    }
    // The id is handed out the first time the game asks for the module, and the visual asks every
    // frame, so -1 only happens before the first camera update.
    if (id == -1 || !vars) return s;
    void* firearm = g.getComponent(vars, id);
    s.aiming = firearm && g.isIronsightAiming(firearm);
    return s;
}

} // namespace DL2HT
