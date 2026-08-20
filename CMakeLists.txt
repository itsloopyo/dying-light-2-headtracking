cmake_minimum_required(VERSION 3.20)
project(DyingLight2HeadTracking VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-specific settings
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Include cameraunlock-core shared library
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

# MinHook sources
set(MINHOOK_SOURCES
    extern/minhook/src/buffer.c
    extern/minhook/src/hook.c
    extern/minhook/src/trampoline.c
    extern/minhook/src/hde/hde32.c
    extern/minhook/src/hde/hde64.c
)

# inih source
set(INIH_SOURCES
    extern/ini.c
)

# ImGui sources
set(IMGUI_SOURCES
    extern/imgui/imgui.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp
    extern/imgui/imgui_impl_win32.cpp
    extern/imgui/imgui_impl_dx12.cpp
)

# Kiero source
set(KIERO_SOURCES
    extern/kiero/kiero.cpp
)

# Mod source files
set(MOD_SOURCES
    src/pch.cpp
    src/dllmain.cpp
    src/core/mod.cpp
    src/core/logger.cpp
    src/core/config.cpp
    src/core/hotkey_utils.cpp
    src/core/path_utils.cpp
    src/hooks/hook_manager.cpp
    src/hooks/engine_camera_hook.cpp
    src/hooks/input_hook.cpp
    src/hooks/dx_hook.cpp
    src/hooks/crosshair_hook.cpp
    src/ui/notification.cpp
)

# Create ASI (DLL)
add_library(DyingLight2HeadTracking SHARED
    ${MINHOOK_SOURCES}
    ${INIH_SOURCES}
    ${IMGUI_SOURCES}
    ${KIERO_SOURCES}
    ${MOD_SOURCES}
)

# Exclude C sources from precompiled headers (pch.h contains C++ headers)
set_source_files_properties(${MINHOOK_SOURCES} ${INIH_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Precompiled header
target_precompile_headers(DyingLight2HeadTracking PRIVATE src/pch.h)

# Output as .asi for Ultimate ASI Loader
set_target_properties(DyingLight2HeadTracking PROPERTIES
    OUTPUT_NAME "DL2HeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# Link cameraunlock core library
target_link_libraries(DyingLight2HeadTracking PRIVATE cameraunlock)

# Include directories
target_include_directories(DyingLight2HeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/minhook/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiero
)

# Windows libraries
if(WIN32)
    target_link_libraries(DyingLight2HeadTracking PRIVATE
        ws2_32
        d3d12
        dxgi
        dxguid
    )
endif()
