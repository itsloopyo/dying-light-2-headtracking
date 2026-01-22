#include "pch.h"
#include "logger.h"
#include "path_utils.h"
#include <cstdio>

namespace DL2HT {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

bool Logger::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    // Get log file path
    std::string logPath = GetModulePath("HeadTracking.log");
    m_logFile.open(logPath, std::ios::out | std::ios::trunc);
    if (!m_logFile.is_open()) {
        return false;
    }

#ifdef _DEBUG
    // Allocate console for debug builds
    if (AllocConsole()) {
        FILE* pFile = nullptr;
        freopen_s(&pFile, "CONOUT$", "w", stdout);
        freopen_s(&pFile, "CONOUT$", "w", stderr);
        m_consoleAllocated = true;
        SetConsoleTitleA("DL2 Head Tracking Debug Console");
    }
    m_minLevel = LogLevel::Debug;
#endif

    m_initialized = true;
    return true;
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_logFile.is_open()) {
        m_logFile.close();
    }

#ifdef _DEBUG
    if (m_consoleAllocated) {
        FreeConsole();
        m_consoleAllocated = false;
    }
#endif

    m_initialized = false;
}

void Logger::LogVa(LogLevel level, const char* fmt, va_list args) {
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    WriteLog(level, buffer);
}

void Logger::Log(LogLevel level, const char* fmt, ...) {
    if (level < m_minLevel) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    LogVa(level, fmt, args);
    va_end(args);
}

void Logger::Debug(const char* fmt, ...) {
    if (LogLevel::Debug < m_minLevel) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    LogVa(LogLevel::Debug, fmt, args);
    va_end(args);
}

void Logger::Info(const char* fmt, ...) {
    if (LogLevel::Info < m_minLevel) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    LogVa(LogLevel::Info, fmt, args);
    va_end(args);
}

void Logger::Warning(const char* fmt, ...) {
    if (LogLevel::Warning < m_minLevel) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    LogVa(LogLevel::Warning, fmt, args);
    va_end(args);
}

void Logger::Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogVa(LogLevel::Error, fmt, args);
    va_end(args);
}

void Logger::WriteLog(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_s(&tm, &time);

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d.%03d",
             tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));

    // Format: [HH:MM:SS.mmm] [LEVEL] Message
    const char* levelStr = LevelToString(level);

    if (m_logFile.is_open()) {
        m_logFile << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
    }

#ifdef _DEBUG
    if (m_consoleAllocated) {
        printf("[%s] [%s] %s\n", timestamp, levelStr, message);
    }
#endif
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

} // namespace DL2HT
