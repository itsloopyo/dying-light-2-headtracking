#include "pch.h"
#include "crosshair_hook.h"
#include "core/logger.h"
#include <Psapi.h>

namespace DL2HT {

// GuiCrosshairData instance and offsets
static std::atomic<void*> g_pGuiCrosshairData{nullptr};
static void* g_pVtable = nullptr;
static bool g_stockCrosshairSuppressed = false;
static bool g_rttiFound = false;

// Module info
static HMODULE g_hGameDll = nullptr;
static BYTE* g_gameBase = nullptr;
static size_t g_gameSize = 0;

// Known member offsets for GuiCrosshairData
// These will need to be found through testing
static constexpr int OFFSET_UNKNOWN = -1;

static bool InitGameModule() {
    if (g_hGameDll) return true;

    g_hGameDll = GetModuleHandleA("gamedll_ph_x64_rwdi.dll");
    if (!g_hGameDll) {
        return false;
    }

    MODULEINFO modInfo = {0};
    if (!GetModuleInformation(GetCurrentProcess(), g_hGameDll, &modInfo, sizeof(modInfo))) {
        return false;
    }

    g_gameBase = (BYTE*)g_hGameDll;
    g_gameSize = modInfo.SizeOfImage;

    return true;
}

// Find string in module
static void* FindString(const char* str) {
    if (!g_gameBase) return nullptr;
    size_t len = strlen(str);
    for (size_t i = 0; i < g_gameSize - len; i++) {
        if (memcmp(g_gameBase + i, str, len) == 0) {
            return g_gameBase + i;
        }
    }
    return nullptr;
}

// Find RTTI type descriptor for a class
static void* FindRTTI(const char* className) {
    char rttiName[256];
    snprintf(rttiName, sizeof(rttiName), ".?AV%s@@", className);
    void* nameAddr = FindString(rttiName);
    if (!nameAddr) return nullptr;
    // TypeDescriptor is 16 bytes before the name string
    return (BYTE*)nameAddr - 16;
}

// Safe memory read helpers
static bool SafeReadDword(void* addr, DWORD* out) {
    __try {
        *out = *(DWORD*)addr;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeReadPtr(void* addr, void** out) {
    __try {
        *out = *(void**)addr;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Find vtable from RTTI type descriptor
static void* FindVtableFromRTTI(void* typeDescriptor) {
    if (!typeDescriptor) {
        Logger::Instance().Error("FindVtableFromRTTI: typeDescriptor is null");
        return nullptr;
    }
    if (!g_gameBase) {
        Logger::Instance().Error("FindVtableFromRTTI: g_gameBase not initialized");
        return nullptr;
    }

    BYTE* typeDesc = (BYTE*)typeDescriptor;
    DWORD typeDescRVA = (DWORD)(typeDesc - g_gameBase);

    // Scan for the COL with matching type descriptor RVA
    for (size_t i = 0; i < g_gameSize - 24; i += 4) {
        DWORD sig, tdRVA, selfRVA;
        if (!SafeReadDword(g_gameBase + i, &sig)) continue;
        if (sig != 1) continue;

        if (!SafeReadDword(g_gameBase + i + 12, &tdRVA)) continue;
        if (tdRVA != typeDescRVA) continue;

        if (!SafeReadDword(g_gameBase + i + 20, &selfRVA)) continue;
        if (selfRVA != (DWORD)i) continue;

        // Valid COL found - now find vtable
        void* col = g_gameBase + i;

        for (size_t j = 8; j < g_gameSize - 8; j += 8) {
            void* ptr;
            if (!SafeReadPtr(g_gameBase + j, &ptr)) continue;
            if (ptr == col) {
                return g_gameBase + j + 8;
            }
        }
    }

    return nullptr;
}

// Find code that references m_DotEnabled string to determine field offset
// Currently returns -1 as offset determination requires manual reverse engineering
static int FindDotEnabledOffset() {
    // Offset determination not yet implemented
    // Stock crosshair control will be a no-op until this is reverse engineered
    return -1;
}

bool InstallCrosshairHook() {
    if (!InitGameModule()) {
        return false;
    }

    // Find RTTI and vtable (safe during init)
    void* rttiTypeDesc = FindRTTI("GuiCrosshairData");
    if (!rttiTypeDesc) {
        Logger::Instance().Error("Could not find GuiCrosshairData RTTI");
        return false;
    }

    g_pVtable = FindVtableFromRTTI(rttiTypeDesc);
    if (!g_pVtable) {
        Logger::Instance().Warning("Could not find GuiCrosshairData vtable");
        return false;
    }

    // Try to find the m_DotEnabled offset (not yet implemented)
    FindDotEnabledOffset();

    g_rttiFound = true;

    // NOTE: Stock crosshair hiding is not implemented yet.
    // SetStockCrosshairVisible() requires knowing the m_DotEnabled offset in GuiCrosshairData.
    // The custom crosshair overlay works independently of this feature.

    return true;
}

void RemoveCrosshairHook() {
    void* crosshairData = g_pGuiCrosshairData.load();
    if (crosshairData && g_stockCrosshairSuppressed) {
        SetStockCrosshairVisible(true);
    }
    g_pGuiCrosshairData.store(nullptr);
    g_pVtable = nullptr;
    g_stockCrosshairSuppressed = false;
}

void SetStockCrosshairVisible(bool visible) {
    (void)visible;
    // Stock crosshair control requires knowing the exact m_DotEnabled offset
    // in GuiCrosshairData. Until that's reverse engineered, this is a no-op.
}

} // namespace DL2HT
