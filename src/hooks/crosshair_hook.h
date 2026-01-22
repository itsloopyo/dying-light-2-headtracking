#pragma once

namespace DL2HT {

// Stock crosshair suppression
// Uses RTTI to find GuiCrosshairData vtable for future m_DotEnabled control
bool InstallCrosshairHook();
void RemoveCrosshairHook();
void SetStockCrosshairVisible(bool visible);

} // namespace DL2HT
