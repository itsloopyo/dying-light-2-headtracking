#pragma once

namespace DL2HT {

class HookManager {
public:
    static HookManager& Instance();

    bool Initialize();
    void Shutdown();
    bool EnableAllHooks();

    HookManager(const HookManager&) = delete;
    HookManager& operator=(const HookManager&) = delete;

private:
    HookManager() = default;
    ~HookManager() = default;

    bool m_initialized = false;
};

} // namespace DL2HT
