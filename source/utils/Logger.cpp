#include "utils/Logger.hpp"

#include <cstdio>
#include <ctime>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace pksm::utils {

bool Logger::initialized = false;
bool Logger::socket_initialized = false;
bool Logger::console_initialized = false;
int Logger::OUTPUT_TO_FILE = 1;
int Logger::ADVANCED_LOGGING = 0;

namespace {

constexpr const char* LOG_DIR = "sdmc:/switch/PKSM";
constexpr const char* LOG_FILE = "sdmc:/switch/PKSM/pksm.log";

std::mutex g_log_mutex;
std::condition_variable g_log_cv;
std::vector<std::string> g_pending_lines;
std::thread g_flush_thread;
bool g_flush_thread_running = false;
bool g_flush_thread_stop = false;
bool g_log_file_cleared = false;

void FlushPendingToFileLocked(std::vector<std::string>& out) {
    if (g_pending_lines.empty()) {
        return;
    }
    out.swap(g_pending_lines);
}

void FlushLinesToFile(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return;
    }

    try {
        std::filesystem::create_directories(LOG_DIR);
        std::ofstream out(LOG_FILE, std::ios::out | std::ios::app);
        if (!out.good()) {
            return;
        }

        for (const auto& line : lines) {
            out.write(line.data(), static_cast<std::streamsize>(line.size()));
        }
        out.flush();
    } catch (...) {
        // ignore file logging failures
    }
}

void FlushThreadMain() {
    std::unique_lock<std::mutex> lk(g_log_mutex);
    while (!g_flush_thread_stop) {
        g_log_cv.wait_for(lk, std::chrono::seconds(3));

        std::vector<std::string> local;
        FlushPendingToFileLocked(local);

        lk.unlock();
        FlushLinesToFile(local);
        lk.lock();
    }

    std::vector<std::string> local;
    FlushPendingToFileLocked(local);
    lk.unlock();
    FlushLinesToFile(local);
}

void EnsureFlushThreadStarted() {
    if (g_flush_thread_running) {
        return;
    }

    try {
        std::filesystem::create_directories(LOG_DIR);
        if (!g_log_file_cleared) {
            std::ofstream out(LOG_FILE, std::ios::out | std::ios::trunc);
            g_log_file_cleared = true;
        } else {
            std::ofstream out(LOG_FILE, std::ios::out | std::ios::app);
        }
    } catch (...) {
        // ignore file logging failures
    }

    g_flush_thread_stop = false;
    g_flush_thread_running = true;
    g_flush_thread = std::thread(FlushThreadMain);
}

void StopFlushThread() {
    if (!g_flush_thread_running) {
        return;
    }

    {
        std::lock_guard<std::mutex> lg(g_log_mutex);
        g_flush_thread_stop = true;
    }
    g_log_cv.notify_all();
    if (g_flush_thread.joinable()) {
        g_flush_thread.join();
    }
    g_flush_thread_running = false;
}

}  // namespace

void Logger::Initialize() {
    if (!initialized) {
#ifndef NDEBUG
        // Initialize socket for nxlink first
        Result rc = socketInitializeDefault();
        socket_initialized = R_SUCCEEDED(rc);

        // Enable logging only when nxlink redirection succeeds.
        // Avoid consoleInit(NULL) fallback since it can conflict with SDL/Plutonium
        // and cause black screens/crashes when launching normally.
        console_initialized = false;
        if (socket_initialized) {
            console_initialized = nxlinkStdio() > 0;
            if (!console_initialized) {
                socketExit();
                socket_initialized = false;
            }
        }
#else
        socket_initialized = false;
        console_initialized = false;
#endif

        initialized = true;

#ifndef NDEBUG
        if (console_initialized) {
            setvbuf(stdout, NULL, _IONBF, 0);
        }
#endif

        if (OUTPUT_TO_FILE != 0) {
            EnsureFlushThreadStarted();
        }
    }
}

void Logger::Finalize() {
    if (!initialized) {
        return;
    }

    StopFlushThread();

#ifndef NDEBUG
    if (socket_initialized) {
        socketExit();
        socket_initialized = false;
    }
#else
    socket_initialized = false;
#endif
    console_initialized = false;
    initialized = false;
}

void Logger::Debug(const std::string& message) {
    Log(Level::Debug, message);
}

void Logger::Info(const std::string& message) {
    Log(Level::Info, message);
}

void Logger::Warning(const std::string& message) {
    Log(Level::Warning, message);
}

void Logger::Error(const std::string& message) {
    Log(Level::Error, message);
}

void Logger::Log(Level level, const std::string& message) {
    if (!initialized) {
        Initialize();
        if (!initialized) {
            return;
        }
    }

    // Get current time
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);

    std::stringstream ss;
    ss << "[" << std::put_time(tm, "%H:%M:%S") << "] ";

    // Add log level
    switch (level) {
        case Level::Debug:
            ss << "[DEBUG] ";
            break;
        case Level::Info:
            ss << "[INFO] ";
            break;
        case Level::Warning:
            ss << "[WARNING] ";
            break;
        case Level::Error:
            ss << "[ERROR] ";
            break;
    }

    ss << message << "\n";
    const std::string line = ss.str();

#ifndef NDEBUG
    if (console_initialized) {
        printf("%s", line.c_str());
        fflush(stdout);

        // Give a small delay after each log to ensure it's flushed
        if (socket_initialized) {
            svcSleepThread(1'000'000);  // 1ms delay
        }
    }
#endif

    if (OUTPUT_TO_FILE != 0) {
        EnsureFlushThreadStarted();
        {
            std::lock_guard<std::mutex> lg(g_log_mutex);
            g_pending_lines.push_back(line);
            if (g_pending_lines.size() >= 128) {
                g_log_cv.notify_all();
            }
        }
    }
}

void Logger::LogMemoryInfo() {
    if (!initialized) {
        Initialize();
        if (!initialized) {
            return;
        }
    }

#ifdef NDEBUG
    return;
#else

    u64 total = 0;
    u64 used = 0;

    // Get total memory
    Result rc = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(rc)) {
        Error("Failed to get total memory size");
        return;
    }

    // Get used memory
    rc = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(rc)) {
        Error("Failed to get used memory size");
        return;
    }

    // Calculate available memory
    u64 available = total - used;

    std::stringstream ss;
    ss << "Memory - Total: " << (total / 1024 / 1024) << "MB, "
       << "Used: " << (used / 1024 / 1024) << "MB, "
       << "Available: " << (available / 1024 / 1024) << "MB";
    Debug(ss.str());
#endif
}

}  // namespace pksm::utils