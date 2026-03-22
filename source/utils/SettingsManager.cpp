#include "utils/SettingsManager.hpp"
#include "utils/Logger.hpp"
#include <fstream>
#include <filesystem>
#include <thread>

namespace pksm {

namespace utils {

SettingsManager& SettingsManager::getInstance() {
    static SettingsManager instance;
    return instance;
}

bool SettingsManager::initialize() {
    LOG_DEBUG("Initializing SettingsManager...");
    
    if (!ensureDirectoryExists("sdmc:/switch/PKSM")) {
        LOG_ERROR("Failed to create PKSM directory");
        return false;
    }
    
    if (!ensureDirectoryExists(cacheDirectory)) {
        LOG_ERROR("Failed to create cache directory");
        return false;
    }
    
    // load existing settings
    if (!loadFromFile()) {
        LOG_INFO("Settings file not found, using defaults");
        settings = nlohmann::json::object();
        
        // set default values
        setString("language", "English");
        setBool("backup_save", true);
        setBool("debug_mode", false);
        
        // save defaults
        save();
    }
    
    LOG_DEBUG("SettingsManager initialized successfully");
    return true;
}

bool SettingsManager::save() {
    try {
        std::ofstream file(settingsFilePath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open settings file for writing: " + settingsFilePath);
            return false;
        }
        
        file << settings.dump(4);
        file.close();
        
        LOG_DEBUG("Settings saved successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while saving settings: " + std::string(e.what()));
        return false;
    }
}

std::string SettingsManager::getString(const std::string& key, const std::string& defaultValue) const {
    if (settings.contains(key) && settings[key].is_string()) {
        return settings[key];
    }
    return defaultValue;
}

bool SettingsManager::getBool(const std::string& key, bool defaultValue) const {
    if (settings.contains(key) && settings[key].is_boolean()) {
        return settings[key];
    }
    return defaultValue;
}

int SettingsManager::getInt(const std::string& key, int defaultValue) const {
    if (settings.contains(key) && settings[key].is_number()) {
        return settings[key];
    }
    return defaultValue;
}

double SettingsManager::getDouble(const std::string& key, double defaultValue) const {
    if (settings.contains(key) && settings[key].is_number()) {
        return settings[key];
    }
    return defaultValue;
}

void SettingsManager::setString(const std::string& key, const std::string& value) {
    settings[key] = value;
}

void SettingsManager::setBool(const std::string& key, bool value) {
    settings[key] = value;
}

void SettingsManager::setInt(const std::string& key, int value) {
    settings[key] = value;
}

void SettingsManager::setDouble(const std::string& key, double value) {
    settings[key] = value;
}

bool SettingsManager::hasKey(const std::string& key) const {
    return settings.contains(key);
}

void SettingsManager::removeKey(const std::string& key) {
    if (settings.contains(key)) {
        settings.erase(key);
    }
}

nlohmann::json SettingsManager::getAllSettings() const {
    return settings;
}

void SettingsManager::clear() {
    settings.clear();
}

size_t SettingsManager::getCacheSize() const {
    return calculateDirectorySize(cacheDirectory);
}

bool SettingsManager::clearCache() {
    try {
        if (std::filesystem::exists(cacheDirectory)) {
            bool success = removeDirectory(cacheDirectory);
            if (success) {
                ensureDirectoryExists(cacheDirectory);
                LOG_INFO("Cache cleared successfully");
            }
            return success;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while clearing cache: " + std::string(e.what()));
        return false;
    }
}

bool SettingsManager::loadFromFile() {
    try {
        if (!std::filesystem::exists(settingsFilePath)) {
            return false;
        }
        
        std::ifstream file(settingsFilePath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open settings file for reading: " + settingsFilePath);
            return false;
        }
        
        try {
            file >> settings;
            file.close();
            LOG_DEBUG("Settings loaded successfully");
            return true;
        } catch (const nlohmann::json::parse_error& e) {
            LOG_ERROR("JSON parse error in settings file: " + std::string(e.what()));
            file.close();
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while loading settings: " + std::string(e.what()));
        return false;
    }
}

bool SettingsManager::ensureDirectoryExists(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
            if (ec) {
                LOG_ERROR("Failed to create directory " + path + ": " + ec.message());
                return false;
            }
            LOG_DEBUG("Created directory: " + path);
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create directory " + path + ": " + std::string(e.what()));
        return false;
    }
}

size_t SettingsManager::calculateDirectorySize(const std::string& path) const {
    size_t totalSize = 0;
    try {
        if (std::filesystem::exists(path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    totalSize += entry.file_size();
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error calculating directory size: " + std::string(e.what()));
    }
    return totalSize;
}

bool SettingsManager::removeDirectory(const std::string& path) {
    try {
        if (std::filesystem::exists(path)) {
            std::error_code ec;
            bool success = std::filesystem::remove(path, ec);
            if (ec) {
                LOG_ERROR("Failed to remove directory " + path + ": " + ec.message());
                return false;
            }
            LOG_DEBUG("Removed directory: " + path);
            return true;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to remove directory " + path + ": " + std::string(e.what()));
        return false;
    }
}

}

} // namespace pksm