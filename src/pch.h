#pragma once

// Windows headers - order matters
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// Standard library
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

// MinHook
#include "MinHook.h"

// Shared constants
#include "core/constants.h"

// Forward declarations
namespace DL2HT {
    class Mod;
    struct Config;
    class Logger;
    class HookManager;
}
