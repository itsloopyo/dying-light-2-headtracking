#include "pch.h"
#include "hook_manager.h"
#include "core/logger.h"

namespace DL2HT {

HookManager& HookManager::Instance() {
    static HookManager instance;
    return instance;
}

bool HookManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        Logger::Instance().Error("MH_Initialize failed: %d", static_cast<int>(status));
        return false;
    }

    m_initialized = true;
    Logger::Instance().Info("MinHook initialized successfully");
    return true;
}

void HookManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Disable all hooks first
    MH_DisableHook(MH_ALL_HOOKS);

    // Uninitialize MinHook
    MH_STATUS status = MH_Uninitialize();
    if (status != MH_OK) {
        Logger::Instance().Warning("MH_Uninitialize failed: %d", static_cast<int>(status));
    }

    m_initialized = false;
    Logger::Instance().Info("MinHook shutdown complete");
}

} // namespace DL2HT
