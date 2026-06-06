#pragma once

namespace DL2HT {

// Install the input polling thread for hotkey detection
bool InstallInputHook();

// Remove the input polling thread
void RemoveInputHook();

} // namespace DL2HT
