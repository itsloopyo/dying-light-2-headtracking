#include "pch.h"
#include "dx_hook.h"
#include "crosshair_hook.h"
#include "engine_camera_hook.h"
#include "core/logger.h"

#include <d3d12.h>
#include <dxgi1_4.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "kiero.h"
#include <cameraunlock/rendering/crosshair_projection.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace DL2HT {

// DX12 hook signatures
typedef void(__stdcall* ExecuteCommandLists_t)(ID3D12CommandQueue* pQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall* Present1_t)(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

// Constants
static constexpr float DEFAULT_FOV_DEG = 75.0f;
static constexpr int DX12_EXECUTECOMMANDLISTS_INDEX = 54;
static constexpr int DX12_PRESENT_INDEX = 140;
static constexpr int DX12_PRESENT1_INDEX = 154;
static constexpr int DX12_RESIZEBUFFERS_INDEX = 145;

// DX12 state - groups all DirectX related objects and state
struct DX12State {
    // Original function pointers for hooks
    ExecuteCommandLists_t oExecuteCommandLists = nullptr;
    Present_t oPresent = nullptr;
    Present1_t oPresent1 = nullptr;
    ResizeBuffers_t oResizeBuffers = nullptr;

    // DX12 objects
    ID3D12Device* pDevice = nullptr;
    ID3D12CommandQueue* pCommandQueue = nullptr;
    ID3D12DescriptorHeap* pSrvDescHeap = nullptr;
    ID3D12CommandAllocator* pCommandAllocator = nullptr;
    ID3D12GraphicsCommandList* pCommandList = nullptr;
    std::vector<ID3D12Resource*> pBackBuffers;
    ID3D12DescriptorHeap* pRtvDescHeap = nullptr;
    UINT rtvDescriptorSize = 0;
    UINT bufferCount = 0;

    // Window
    HWND hWindow = nullptr;
    WNDPROC oWndProc = nullptr;

    // Cached SwapChain3 interface (avoids per-frame QueryInterface)
    IDXGISwapChain3* pSwapChain3 = nullptr;

    // State flags
    bool initialized = false;
    bool hookInstalled = false;
    bool commandQueueReady = false;
    bool execCmdListsCalled = false;
    bool firstPresentCall = true;
};

// Crosshair state - atomic values for thread-safe access
// Using relaxed memory ordering - single producer (UpdateTrackingData) / single consumer (DrawCrosshair)
struct CrosshairState {
    std::atomic<float> yawOffset{0.0f};
    std::atomic<float> pitchOffset{0.0f};
    std::atomic<float> rollOffset{0.0f};
    std::atomic<float> gameCameraPitch{0.0f};  // Game camera pitch (radians) from gamepad/mouse
    std::atomic<bool> enabled{false};

    // Config - match stock reticle (small white dot)
    float dotSize = 3.0f;
    ImU32 color = IM_COL32(255, 255, 255, 255);
};

// Global state instances
static DX12State g_dx;
static CrosshairState g_crosshair;

// Forward declarations
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return CallWindowProc(g_dx.oWndProc, hWnd, msg, wParam, lParam);
}

// Update per-frame state from DX Present.
// Rotation, position, and crosshair offset are processed directly in
// MoveCameraHook for per-camera-update smoothing/interpolation.
static void UpdateTrackingData() {
    RefreshGameplayStateCache();
}

// Disable optimization for DrawCrosshair to work around MSVC 2022 internal compiler error
#pragma optimize("", off)
static void DrawCrosshair(float screenWidth, float screenHeight) {
    // Use relaxed ordering - these values are written by UpdateTrackingData and read here
    // No synchronization needed, just visibility of latest values
    if (!g_crosshair.enabled.load(std::memory_order_relaxed)) return;

    // Only show crosshair during gameplay (not menus, cutscenes, loading)
    if (!IsInGameplay()) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    // Use core library for crosshair projection
    // Batch atomic loads with relaxed ordering for minimal overhead
    cameraunlock::rendering::CrosshairProjectionParams params;
    params.screenWidth = screenWidth;
    params.screenHeight = screenHeight;
    params.fovDegrees = DEFAULT_FOV_DEG;
    params.yawOffset = g_crosshair.yawOffset.load(std::memory_order_relaxed);
    params.pitchOffset = g_crosshair.pitchOffset.load(std::memory_order_relaxed);
    params.rollOffset = g_crosshair.rollOffset.load(std::memory_order_relaxed);
    params.gameCameraPitch = g_crosshair.gameCameraPitch.load(std::memory_order_relaxed);

    cameraunlock::rendering::ScreenPosition pos = cameraunlock::rendering::ProjectCrosshair(params);

    if (!pos.valid) return;

    // Clamp to screen
    float margin = g_crosshair.dotSize + 10.0f;
    cameraunlock::rendering::ClampToScreen(pos, screenWidth, screenHeight, margin);

    // Simple white dot to match stock reticle
    drawList->AddCircleFilled(ImVec2(pos.x, pos.y), g_crosshair.dotSize, g_crosshair.color);
}
#pragma optimize("", on)

static bool InitializeDX12(IDXGISwapChain* pSwapChain) {
    if (g_dx.initialized) return true;
    if (!g_dx.commandQueueReady || !g_dx.pCommandQueue) {
        return false;
    }

    Logger::Instance().Info("InitializeDX12: Starting...");

    // Get device from command queue
    HRESULT hr = g_dx.pCommandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&g_dx.pDevice);
    if (FAILED(hr) || !g_dx.pDevice) {
        Logger::Instance().Error("Failed to get D3D12 device: 0x%08X", hr);
        return false;
    }

    // Get swapchain description
    DXGI_SWAP_CHAIN_DESC desc;
    hr = pSwapChain->GetDesc(&desc);
    if (FAILED(hr)) {
        Logger::Instance().Error("Failed to get swapchain desc: 0x%08X", hr);
        return false;
    }

    g_dx.hWindow = desc.OutputWindow;
    g_dx.bufferCount = desc.BufferCount;

    // Cache SwapChain3 interface for per-frame GetCurrentBackBufferIndex
    pSwapChain->QueryInterface(IID_PPV_ARGS(&g_dx.pSwapChain3));

    Logger::Instance().Info("Swapchain: %dx%d, %d buffers, window=%p",
        desc.BufferDesc.Width, desc.BufferDesc.Height, g_dx.bufferCount, g_dx.hWindow);

    // Create SRV descriptor heap for ImGui
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = g_dx.pDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_dx.pSrvDescHeap));
    if (FAILED(hr)) {
        Logger::Instance().Error("Failed to create SRV heap: 0x%08X", hr);
        return false;
    }

    // Create RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = g_dx.bufferCount;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = g_dx.pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_dx.pRtvDescHeap));
    if (FAILED(hr)) {
        Logger::Instance().Error("Failed to create RTV heap: 0x%08X", hr);
        return false;
    }
    g_dx.rtvDescriptorSize = g_dx.pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create command allocator
    hr = g_dx.pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_dx.pCommandAllocator));
    if (FAILED(hr)) {
        Logger::Instance().Error("Failed to create command allocator: 0x%08X", hr);
        return false;
    }

    // Create command list
    hr = g_dx.pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_dx.pCommandAllocator, nullptr, IID_PPV_ARGS(&g_dx.pCommandList));
    if (FAILED(hr)) {
        Logger::Instance().Error("Failed to create command list: 0x%08X", hr);
        return false;
    }
    g_dx.pCommandList->Close();

    // Get back buffers and create RTVs
    g_dx.pBackBuffers.resize(g_dx.bufferCount, nullptr);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_dx.pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < g_dx.bufferCount; i++) {
        hr = pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_dx.pBackBuffers[i]));
        if (FAILED(hr)) {
            Logger::Instance().Error("Failed to get back buffer %d: 0x%08X", i, hr);
            return false;
        }
        g_dx.pDevice->CreateRenderTargetView(g_dx.pBackBuffers[i], nullptr, rtvHandle);
        rtvHandle.ptr += g_dx.rtvDescriptorSize;
    }

    // Hook WndProc
    g_dx.oWndProc = (WNDPROC)SetWindowLongPtr(g_dx.hWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);

    // Initialize ImGui
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(g_dx.hWindow);

    // Initialize ImGui DX12 backend using InitInfo struct (includes command queue for font upload)
    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = g_dx.pDevice;
    initInfo.CommandQueue = g_dx.pCommandQueue;
    initInfo.NumFramesInFlight = g_dx.bufferCount;
    initInfo.RTVFormat = desc.BufferDesc.Format;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = g_dx.pSrvDescHeap;
    initInfo.LegacySingleSrvCpuDescriptor = g_dx.pSrvDescHeap->GetCPUDescriptorHandleForHeapStart();
    initInfo.LegacySingleSrvGpuDescriptor = g_dx.pSrvDescHeap->GetGPUDescriptorHandleForHeapStart();

    if (!ImGui_ImplDX12_Init(&initInfo)) {
        Logger::Instance().Error("ImGui_ImplDX12_Init failed");
        return false;
    }

    g_dx.initialized = true;
    Logger::Instance().Info("DX12 ImGui initialized successfully");

    // Crosshair data scan is done in RenderImGui (retries across frames)

    return true;
}

static void RenderImGui(IDXGISwapChain* pSwapChain) {
    if (!g_dx.initialized) return;

    // Get current back buffer index (using cached SwapChain3 interface)
    UINT bufferIndex = 0;
    if (g_dx.pSwapChain3) {
        bufferIndex = g_dx.pSwapChain3->GetCurrentBackBufferIndex();
    }

    // Reset command allocator and list
    g_dx.pCommandAllocator->Reset();
    g_dx.pCommandList->Reset(g_dx.pCommandAllocator, nullptr);

    // Transition back buffer to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = g_dx.pBackBuffers[bufferIndex];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_dx.pCommandList->ResourceBarrier(1, &barrier);

    // Set render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_dx.pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += bufferIndex * g_dx.rtvDescriptorSize;
    g_dx.pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Set descriptor heaps for ImGui
    ID3D12DescriptorHeap* heaps[] = { g_dx.pSrvDescHeap };
    g_dx.pCommandList->SetDescriptorHeaps(1, heaps);

    // Start ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Draw crosshair
    ImGuiIO& io = ImGui::GetIO();
    DrawCrosshair(io.DisplaySize.x, io.DisplaySize.y);

    // Render ImGui
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_dx.pCommandList);

    // Transition back buffer to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_dx.pCommandList->ResourceBarrier(1, &barrier);

    // Close and execute
    g_dx.pCommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { g_dx.pCommandList };
    g_dx.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
}

// Shared Present hook logic - called by both hkPresent and hkPresent1
static void ProcessPresentFrame(IDXGISwapChain* pSwapChain, const char* hookName) {
    if (g_dx.firstPresentCall) {
        g_dx.firstPresentCall = false;
        Logger::Instance().Info("%s: first call", hookName);
    }

    UpdateTrackingData();

    if (!g_dx.initialized && g_dx.commandQueueReady) {
        if (InitializeDX12(pSwapChain)) {
            Logger::Instance().Info("DX12 initialized via %s", hookName);
        }
    }

    if (g_dx.initialized) {
        RenderImGui(pSwapChain);
    }
}

static void __stdcall hkExecuteCommandLists(ID3D12CommandQueue* pQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
    if (!g_dx.commandQueueReady && pQueue) {
        // Verify this is a direct command queue (type 0)
        D3D12_COMMAND_QUEUE_DESC desc = pQueue->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            g_dx.pCommandQueue = pQueue;
            g_dx.commandQueueReady = true;
            if (!g_dx.execCmdListsCalled) {
                g_dx.execCmdListsCalled = true;
                Logger::Instance().Info("Captured DX12 command queue (type=%d)", desc.Type);
            }
        }
    }

    g_dx.oExecuteCommandLists(pQueue, NumCommandLists, ppCommandLists);
}

static HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    ProcessPresentFrame(pSwapChain, "hkPresent");
    return g_dx.oPresent(pSwapChain, SyncInterval, Flags);
}

static HRESULT __stdcall hkPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    ProcessPresentFrame(pSwapChain, "hkPresent1");
    return g_dx.oPresent1(pSwapChain, SyncInterval, Flags, pPresentParameters);
}

static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount,
    UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {

    Logger::Instance().Info("ResizeBuffers: %dx%d", Width, Height);

    // Release back buffers before resize
    for (auto& buf : g_dx.pBackBuffers) {
        if (buf) {
            buf->Release();
            buf = nullptr;
        }
    }

    HRESULT hr = g_dx.oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    // Recreate back buffer RTVs
    if (SUCCEEDED(hr) && g_dx.initialized && !g_dx.pBackBuffers.empty()) {
        // BufferCount == 0 means "keep existing count" per DXGI spec
        if (BufferCount > 0) {
            g_dx.bufferCount = BufferCount;
        }
        g_dx.pBackBuffers.resize(g_dx.bufferCount, nullptr);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_dx.pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < g_dx.bufferCount; i++) {
            pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_dx.pBackBuffers[i]));
            g_dx.pDevice->CreateRenderTargetView(g_dx.pBackBuffers[i], nullptr, rtvHandle);
            rtvHandle.ptr += g_dx.rtvDescriptorSize;
        }
    }

    return hr;
}

bool InstallDXHook() {
    if (g_dx.hookInstalled) {
        Logger::Instance().Info("DX hook already installed");
        return true;
    }

    Logger::Instance().Info("Attempting DX12 hook...");
    if (kiero::init(kiero::RenderType::D3D12) != kiero::Status::Success) {
        Logger::Instance().Error("Kiero DX12 init failed");
        return false;
    }

    bool hooked = false;

    // Hook ExecuteCommandLists to capture command queue
    if (kiero::bind(DX12_EXECUTECOMMANDLISTS_INDEX, (void**)&g_dx.oExecuteCommandLists, hkExecuteCommandLists) == kiero::Status::Success) {
        Logger::Instance().Info("DX12 ExecuteCommandLists hooked at index %d", DX12_EXECUTECOMMANDLISTS_INDEX);
        hooked = true;
    }

    // Hook Present
    if (kiero::bind(DX12_PRESENT_INDEX, (void**)&g_dx.oPresent, hkPresent) == kiero::Status::Success) {
        Logger::Instance().Info("DX12 Present hooked at index %d", DX12_PRESENT_INDEX);
        hooked = true;
    }

    // Hook Present1 (DX12 games often use this)
    auto status = kiero::bind(DX12_PRESENT1_INDEX, (void**)&g_dx.oPresent1, hkPresent1);
    if (status == kiero::Status::Success) {
        Logger::Instance().Info("DX12 Present1 hooked at index %d", DX12_PRESENT1_INDEX);
        hooked = true;
    } else {
        Logger::Instance().Warning("DX12 Present1 hook FAILED at index %d (status=%d)", DX12_PRESENT1_INDEX, (int)status);
    }

    // Hook ResizeBuffers
    if (kiero::bind(DX12_RESIZEBUFFERS_INDEX, (void**)&g_dx.oResizeBuffers, hkResizeBuffers) == kiero::Status::Success) {
        Logger::Instance().Info("DX12 ResizeBuffers hooked at index %d", DX12_RESIZEBUFFERS_INDEX);
    }

    if (hooked) {
        g_dx.hookInstalled = true;
        Logger::Instance().Info("DX12 hook installed successfully");
        return true;
    }

    Logger::Instance().Error("Failed to install DX12 hooks");
    kiero::shutdown();
    return false;
}

void RemoveDXHook() {
    if (!g_dx.hookInstalled) return;

    // Shutdown ImGui
    if (g_dx.initialized) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        // Restore WndProc
        if (g_dx.oWndProc && g_dx.hWindow) {
            SetWindowLongPtr(g_dx.hWindow, GWLP_WNDPROC, (LONG_PTR)g_dx.oWndProc);
        }

        // Release resources
        for (auto& buf : g_dx.pBackBuffers) {
            if (buf) buf->Release();
        }
        g_dx.pBackBuffers.clear();

        if (g_dx.pCommandList) { g_dx.pCommandList->Release(); g_dx.pCommandList = nullptr; }
        if (g_dx.pCommandAllocator) { g_dx.pCommandAllocator->Release(); g_dx.pCommandAllocator = nullptr; }
        if (g_dx.pRtvDescHeap) { g_dx.pRtvDescHeap->Release(); g_dx.pRtvDescHeap = nullptr; }
        if (g_dx.pSrvDescHeap) { g_dx.pSrvDescHeap->Release(); g_dx.pSrvDescHeap = nullptr; }
        if (g_dx.pSwapChain3) { g_dx.pSwapChain3->Release(); g_dx.pSwapChain3 = nullptr; }
        if (g_dx.pDevice) { g_dx.pDevice->Release(); g_dx.pDevice = nullptr; }

        g_dx.initialized = false;
    }

    // Unhook
    kiero::unbind(DX12_EXECUTECOMMANDLISTS_INDEX);
    kiero::unbind(DX12_PRESENT_INDEX);
    kiero::unbind(DX12_PRESENT1_INDEX);
    kiero::unbind(DX12_RESIZEBUFFERS_INDEX);
    kiero::shutdown();

    g_dx.hookInstalled = false;
    g_dx.commandQueueReady = false;
    Logger::Instance().Info("DX12 hook removed");
}

void SetCrosshairOffset(float yaw, float pitch, float roll) {
    // Relaxed ordering - single producer, values just need to be visible eventually
    g_crosshair.yawOffset.store(yaw, std::memory_order_relaxed);
    g_crosshair.pitchOffset.store(pitch, std::memory_order_relaxed);
    g_crosshair.rollOffset.store(roll, std::memory_order_relaxed);
}

void SetGameCameraPitch(float pitchRadians) {
    g_crosshair.gameCameraPitch.store(pitchRadians, std::memory_order_relaxed);
}

void SetCrosshairEnabled(bool enabled) {
    g_crosshair.enabled.store(enabled, std::memory_order_relaxed);
}

} // namespace DL2HT
