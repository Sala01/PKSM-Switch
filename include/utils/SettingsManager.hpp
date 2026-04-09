#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

namespace pksm
{
    namespace utils
    {

        class SettingsManager
        {
        public:
            static SettingsManager &getInstance();

            bool initialize();

            bool save();

            std::string getString(const std::string &key, const std::string &defaultValue = "") const;
            bool getBool(const std::string &key, bool defaultValue = false) const;
            int getInt(const std::string &key, int defaultValue = 0) const;
            double getDouble(const std::string &key, double defaultValue = 0.0) const;

            void setString(const std::string &key, const std::string &value);
            void setBool(const std::string &key, bool value);
            void setInt(const std::string &key, int value);
            void setDouble(const std::string &key, double value);

            bool hasKey(const std::string &key) const;

            void removeKey(const std::string &key);

            nlohmann::json getAllSettings() const;

            void clear();

            // cache size in bytes
            size_t getCacheSize() const;

            bool clearCache();

            const std::string &getCacheDirectory() const { return cacheDirectory; }

        private:
            SettingsManager() = default;
            ~SettingsManager() = default;
            SettingsManager(const SettingsManager &) = delete;
            SettingsManager &operator=(const SettingsManager &) = delete;

            nlohmann::json settings;
            std::string settingsFilePath = "sdmc:/switch/PKSM/settings.json";
            std::string cacheDirectory = "sdmc:/switch/PKSM/temp";

            bool loadFromFile();

            bool ensureDirectoryExists(const std::string &path);

            size_t calculateDirectorySize(const std::string &path) const;

            bool removeDirectory(const std::string &path);
        };

    }

} // namespace pksm