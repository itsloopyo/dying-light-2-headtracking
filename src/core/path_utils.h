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

} // namespace DL2HT
