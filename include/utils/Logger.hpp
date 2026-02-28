#pragma once

#include <string>
#include <switch.h>

namespace pksm::utils
{

    class Logger
    {
    public:
        enum class Level
        {
            Debug,
            Info,
            Warning,
            Error
        };

        static void Initialize();
        static void Finalize();
        static void Debug(const std::string &message);
        static void Info(const std::string &message);
        static void Warning(const std::string &message);
        static void Error(const std::string &message);
        static void LogMemoryInfo();

        static int OUTPUT_TO_FILE;

    private:
        static void Log(Level level, const std::string &message);
        static bool initialized;
        static bool socket_initialized;
        static bool console_initialized;
    };

} // namespace pksm::utils

#define LOG_DEBUG(msg) pksm::utils::Logger::Debug(msg)
#define LOG_INFO(msg) pksm::utils::Logger::Info(msg)
#define LOG_WARNING(msg) pksm::utils::Logger::Warning(msg)
#define LOG_ERROR(msg) pksm::utils::Logger::Error(msg)
#define LOG_MEMORY() pksm::utils::Logger::LogMemoryInfo()