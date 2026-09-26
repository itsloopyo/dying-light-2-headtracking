#pragma once

#include <string>
#include <Windows.h>

namespace DL2HT {

// Get the directory containing our DLL
// Returns empty string on failure
std::string GetModuleDirectory();

// Get full path to a file in the same directory as our DLL
// Returns empty string on failure
std::string GetModulePath(const char* filename);

// The directory containing our DLL, ending in its separator, as a wide string so a folder the
// ANSI code page cannot spell is still found. Empty when the module's own path cannot be read.
std::wstring GetModuleDirectoryW();

} // namespace DL2HT
