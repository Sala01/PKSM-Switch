#include "data/providers/SaveDataAccessor.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>

#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>

#include <switch.h>

#include "pksmcore/enums/GameVersion.hpp"
#include "pksmcore/enums/Generation.hpp"
#include "pksmcore/enums/Gender.hpp"
#include "pksmcore/sav/Sav.hpp"
#include "utils/Logger.hpp"
#include "utils/SettingsManager.hpp"

using namespace pksm;

namespace {

pksm::saves::Generation ToAppGeneration(pksm::Generation gen) {
    switch (gen) {
        case pksm::Generation::ONE:
            return pksm::saves::Generation::ONE;
        case pksm::Generation::TWO:
            return pksm::saves::Generation::TWO;
        case pksm::Generation::THREE:
            return pksm::saves::Generation::THREE;
        case pksm::Generation::FOUR:
            return pksm::saves::Generation::FOUR;
        case pksm::Generation::FIVE:
            return pksm::saves::Generation::FIVE;
        case pksm::Generation::SIX:
            return pksm::saves::Generation::SIX;
        case pksm::Generation::SEVEN:
            return pksm::saves::Generation::SEVEN;
        case pksm::Generation::LGPE:
            return pksm::saves::Generation::LGPE;
        case pksm::Generation::EIGHT:
            return pksm::saves::Generation::EIGHT;
        case pksm::Generation::NINE:
            return pksm::saves::Generation::NINE;
        default:
            return pksm::saves::Generation::EIGHT;
    }

}

pksm::saves::BagPouch ToAppBagPouch(const pksm::Sav::Pouch pouch) {
    using SP = pksm::Sav::Pouch;
    using AP = pksm::saves::BagPouch;

    switch (pouch) {
        case SP::NormalItem:
            return AP::NormalItem;
        case SP::KeyItem:
            return AP::KeyItem;
        case SP::TM:
            return AP::TM;
        case SP::Mail:
            return AP::Mail;
        case SP::Medicine:
            return AP::Medicine;
        case SP::Berry:
            return AP::Berry;
        case SP::Ball:
            return AP::Ball;
        case SP::Battle:
            return AP::Battle;
        case SP::Candy:
            return AP::Candy;
        case SP::ZCrystals:
            return AP::ZCrystals;
        case SP::Treasure:
            return AP::Treasure;
        case SP::Ingredient:
            return AP::Ingredient;
        case SP::PCItem:
            return AP::PCItem;
        case SP::RotomPower:
            return AP::RotomPower;
        case SP::CatchingItem:
            return AP::CatchingItem;
        case SP::MegaStones:
            return AP::MegaStones;
        default:
            return AP::Unknown;
    }
}

pksm::saves::Gender ToAppGender(pksm::Gender gender) {
    switch (gender) {
        case pksm::Gender::Female:
            return pksm::saves::Gender::Female;
        case pksm::Gender::Male:
        default:
            return pksm::saves::Gender::Male;
    }
}

pksm::saves::GameVersion ToAppGameVersion(pksm::GameVersion version) {
    using GV = pksm::GameVersion;
    using AGV = pksm::saves::GameVersion;

    switch (version) {
        case GV::RD:
            return AGV::RD;
        case GV::GN:
            return AGV::GN;
        case GV::BU:
            return AGV::BU;
        case GV::YW:
            return AGV::YW;
        case GV::GD:
            return AGV::GD;
        case GV::SV:
            return AGV::SV;
        case GV::C:
            return AGV::C;
        case GV::R:
            return AGV::R;
        case GV::S:
            return AGV::S;
        case GV::E:
            return AGV::E;
        case GV::FR:
            return AGV::FR;
        case GV::LG:
            return AGV::LG;
        case GV::D:
            return AGV::D;
        case GV::P:
            return AGV::P;
        case GV::Pt:
            return AGV::Pt;
        case GV::HG:
            return AGV::HG;
        case GV::SS:
            return AGV::SS;
        case GV::W:
            return AGV::W;
        case GV::B:
            return AGV::B;
        case GV::W2:
            return AGV::W2;
        case GV::B2:
            return AGV::B2;
        case GV::X:
            return AGV::X;
        case GV::Y:
            return AGV::Y;
        case GV::OR:
            return AGV::OR;
        case GV::AS:
            return AGV::AS;
        case GV::SN:
            return AGV::SN;
        case GV::MN:
            return AGV::MN;
        case GV::US:
            return AGV::US;
        case GV::UM:
            return AGV::UM;
        case GV::GP:
            return AGV::GP;
        case GV::GE:
            return AGV::GE;
        case GV::SW:
            return AGV::SW;
        case GV::SH:
            return AGV::SH;
        case GV::PLA:
            return AGV::PLA;
        case GV::SL:
            return AGV::SL;
        case GV::VL:
            return AGV::VL;
        case GV::ZA:
            return AGV::ZA;
        default:
            return AGV::SW;
    }
}

bool CopyFileBinary(const std::string& src, const std::string& dst) {
    std::FILE* in = std::fopen(src.c_str(), "rb");
    if (in == nullptr) {
        const int e = errno;
        LOG_WARNING("Backup copy: failed to open source: " + src + " (errno " + std::to_string(e) + ": " + std::string(std::strerror(e)) + ")");
        return false;
    }

    std::FILE* out = std::fopen(dst.c_str(), "wb");
    if (out == nullptr) {
        const int e = errno;
        std::fclose(in);
        LOG_WARNING("Backup copy: failed to open destination: " + dst + " (errno " + std::to_string(e) + ": " + std::string(std::strerror(e)) + ")");
        return false;
    }

    std::array<unsigned char, 0x4000> buf{};
    std::uintmax_t total = 0;
    while (true) {
        errno = 0;
        const size_t got = std::fread(buf.data(), 1, buf.size(), in);
        if (got == 0) {
            if (std::ferror(in) != 0) {
                const int e = errno;
                std::fclose(in);
                std::fclose(out);
                LOG_WARNING("Backup copy: read failed from source: " + src + " (errno " + std::to_string(e) + ": " + std::string(std::strerror(e)) + ")");
                return false;
            }
            break;
        }
        const size_t wrote = std::fwrite(buf.data(), 1, got, out);
        if (wrote != got) {
            const int e = errno;
            std::fclose(in);
            std::fclose(out);
            LOG_WARNING("Backup copy: write failed to destination: " + dst + " (errno " + std::to_string(e) + ": " + std::string(std::strerror(e)) + ")");
            return false;
        }
        total += static_cast<std::uintmax_t>(wrote);
    }

    std::fflush(out);
    std::fclose(in);
    std::fclose(out);

    if (total == 0) {
        LOG_WARNING("Backup copy: copied 0 bytes (src: " + src + ", dst: " + dst + ")");
        return false;
    }

    LOG_DEBUG("Backup_copy_copied_" + std::to_string(total) + "_bytes_to_" + dst);
    return true;
}

std::string SanitizeForPathSegment(std::string s) {
    std::string result;
    result.reserve(s.length());
    
    // just sanitize the whole thing the switch can only handle ascii
    for (auto& ch : s) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result += ch;
        } else {
            result += '_';
        }
    }
    
    size_t pos = 0;
    while ((pos = result.find("__", pos)) != std::string::npos) {
        result.replace(pos, 2, "_");
    }
    
    if (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    
    return result;
}

std::string MakeTimestampForPath() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    #if defined(_WIN32)
    localtime_s(&tm, &tt);
    #else
    tm = *std::localtime(&tt);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

}  // namespace

SaveDataAccessor::SaveDataAccessor(AccountUid currentUserId)
  : currentSave(nullptr), hasChanges(false), lastError(""), currentSavePath(""), currentUserId(currentUserId) {}

void SaveDataAccessor::unmountSaveDevice() {
    LOG_DEBUG("Unmounting save device");
    Result rc = fsdevUnmountDevice("save");
    if (R_FAILED(rc)) {
        std::stringstream hexStream;
        hexStream << std::hex << rc;
        LOG_DEBUG("Save device was not mounted or unmount failed: " + std::to_string(rc) + " (0x" + hexStream.str() + ")");
    } else {
        LOG_DEBUG("Successfully unmounted save device");
    }
}

pksm::saves::SaveData::Ref SaveDataAccessor::getCurrentSaveData() const {
    return currentSave;
}

bool SaveDataAccessor::loadSave(const pksm::titles::Title::Ref title, const std::string saveName) {
    if (!title) {
        return false;
    }

    auto loaded = LoadSaveDataFromFile(title, saveName);
    if (!loaded) {
        this->unmountSaveDevice();
        return false;
    }

    currentSave = loaded;
    hasChanges = false;
    lastError.clear();
    const bool isSaveDevicePath = saveName.rfind("save:/", 0) == 0;
    const bool isAbsoluteDevicePath = (saveName.find(":/") != std::string::npos);
    currentSavePath = (isSaveDevicePath || isAbsoluteDevicePath) ? saveName : (std::string("save:/") + saveName);

    if (onSaveDataChanged) {
        onSaveDataChanged(currentSave);
    }

    return true;
}

bool SaveDataAccessor::saveChanges() {
    lastError.clear();

    if (!currentSave) {
        lastError = "No save data to save";
        LOG_ERROR(lastError);
        return false;
    }

    const auto savePath = !currentSavePath.empty() ? currentSavePath : currentSave->getName();
    const bool isSaveDevicePath = savePath.rfind("save:", 0) == 0;
    const bool isAbsoluteDevicePath = (savePath.find(":/") != std::string::npos);
    if (!isSaveDevicePath && !isAbsoluteDevicePath) {
        lastError = "Refusing to write save changes to a non-save and non-absolute path: " + savePath;
        LOG_ERROR(lastError);
        return false;
    }

    try {
        std::ifstream in(savePath, std::ios::binary | std::ios::ate);
        if (!in.good()) {
            lastError = "Failed to open save file: " + savePath;
            LOG_ERROR(lastError);
            return false;
        }

        const std::streamsize file_size = in.tellg();
        if (file_size <= 0) {
            lastError = "Invalid save file size: " + savePath;
            LOG_ERROR(lastError);
            return false;
        }

        in.seekg(0, std::ios::beg);

        std::vector<u8> buffer_vec;
        buffer_vec.resize(static_cast<size_t>(file_size));
        if (!in.read(reinterpret_cast<char*>(buffer_vec.data()), file_size)) {
            lastError = "Failed to read save file for writing: " + savePath;
            LOG_ERROR(lastError);
            return false;
        }

        in.close();

        auto buffer = std::shared_ptr<u8[]>(new u8[static_cast<size_t>(file_size)], std::default_delete<u8[]>());
        std::copy(buffer_vec.begin(), buffer_vec.end(), buffer.get());

        auto sav = pksm::Sav::getSave(buffer, static_cast<size_t>(file_size));
        if (!sav) {
            lastError = "PKSM-Core could not detect a valid save type for: " + savePath;
            LOG_ERROR(lastError);
            return false;
        }

        sav->beginEditing();

        const auto bag_items = currentSave->getBagItems();
        const auto pouches = sav->pouches();

        std::map<pksm::Sav::Pouch, int> pouch_slot_counts;
        for (const auto &pouch_info : pouches) {
            pouch_slot_counts[pouch_info.first] = pouch_info.second;
        }

        for (const auto &pouch_info : pouches) {
            const auto pouch = pouch_info.first;
            const auto slot_count = pouch_info.second;
            for (int slot = 0; slot < slot_count; slot++) {
                auto it = sav->item(pouch, static_cast<u16>(slot));
                if (!it) {
                    continue;
                }
                it->id(0);
                it->count(0);
                sav->item(*it, pouch, static_cast<u16>(slot));
            }
        }

        auto toSavPouch = [](const pksm::saves::BagPouch pouch) -> std::optional<pksm::Sav::Pouch> {
            using AP = pksm::saves::BagPouch;
            using SP = pksm::Sav::Pouch;
            switch (pouch) {
                case AP::NormalItem:
                    return SP::NormalItem;
                case AP::KeyItem:
                    return SP::KeyItem;
                case AP::TM:
                    return SP::TM;
                case AP::Mail:
                    return SP::Mail;
                case AP::Medicine:
                    return SP::Medicine;
                case AP::Berry:
                    return SP::Berry;
                case AP::Ball:
                    return SP::Ball;
                case AP::Battle:
                    return SP::Battle;
                case AP::Candy:
                    return SP::Candy;
                case AP::ZCrystals:
                    return SP::ZCrystals;
                case AP::Treasure:
                    return SP::Treasure;
                case AP::Ingredient:
                    return SP::Ingredient;
                case AP::PCItem:
                    return SP::PCItem;
                case AP::RotomPower:
                    return SP::RotomPower;
                case AP::CatchingItem:
                    return SP::CatchingItem;
                case AP::MegaStones:
                    return SP::MegaStones;
                case AP::Unknown:
                default:
                    return std::nullopt;
            }
        };

        std::map<pksm::Sav::Pouch, int> next_slot;
        for (const auto &it : bag_items) {
            if ((it.itemId == 0) || (it.count == 0)) {
                continue;
            }

            const auto sav_pouch_opt = toSavPouch(it.pouch);
            if (!sav_pouch_opt) {
                continue;
            }
            const auto sav_pouch = *sav_pouch_opt;

            const auto slot_count_it = pouch_slot_counts.find(sav_pouch);
            if (slot_count_it == pouch_slot_counts.end()) {
                continue;
            }

            const int slot_count = slot_count_it->second;
            const int slot = next_slot[sav_pouch]++;
            if (slot >= slot_count) {
                LOG_WARNING("Bag pouch full, skipping item ID " + std::to_string(it.itemId));
                continue;
            }

            auto item = sav->item(sav_pouch, static_cast<u16>(slot));
            if (!item) {
                continue;
            }

            item->id(it.itemId);
            item->count(it.count);
            sav->item(*item, sav_pouch, static_cast<u16>(slot));
        }

        sav->finishEditing();

        const size_t out_size = static_cast<size_t>(sav->getEntireLengthIncludingFooter());
        if (out_size != static_cast<size_t>(file_size)) {
            LOG_WARNING("Save size changed after editing. Input size = " + std::to_string(file_size) + ", output size = " + std::to_string(out_size));
        }

        std::ofstream out(savePath, std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            lastError = "Failed to open save file for writing: " + savePath + " (errno " + std::to_string(errno) + ": " + std::string(std::strerror(errno)) + ")";
            LOG_ERROR(lastError);
            return false;
        }

        out.write(reinterpret_cast<const char*>(sav->rawData().get()), static_cast<std::streamsize>(out_size));
        if (!out.good()) {
            lastError = "Failed to write save file: " + savePath + " (errno " + std::to_string(errno) + ": " + std::string(std::strerror(errno)) + ")";
            LOG_ERROR(lastError);
            return false;
        }
        out.flush();
        out.close();

        if (isSaveDevicePath) {
            const Result commit_rc = fsdevCommitDevice("save");
            if (R_FAILED(commit_rc)) {
                std::stringstream hexStream;
                hexStream << std::hex << commit_rc;
                lastError = "Failed to commit save device: " + std::to_string(commit_rc) + " (0x" + hexStream.str() + ")";
                LOG_ERROR(lastError);
                return false;
            }
        }

        hasChanges = false;
        lastError.clear();
        LOG_DEBUG("Successfully saved changes to: " + savePath);
        return true;
    } catch (const std::exception &e) {
        lastError = std::string("SaveChanges failed: ") + e.what();
        LOG_ERROR(lastError);
        return false;
    }
}

std::string SaveDataAccessor::getLastError() const {
    return lastError;
}

void SaveDataAccessor::markUnsavedChanges() {
    hasChanges = true;
}

bool SaveDataAccessor::hasUnsavedChanges() const {
    return hasChanges;
}

pksm::saves::SaveData::Ref SaveDataAccessor::LoadSaveDataFromFile(
    const pksm::titles::Title::Ref& title,
    const std::string& saveName
) const {
    if (!title) {
        return nullptr;
    }

    const bool isSaveDevicePathArg = saveName.rfind("save:/", 0) == 0;
    const bool isAbsoluteDevicePathArg = (saveName.find(":/") != std::string::npos);
    const std::string savePath = (isSaveDevicePathArg || isAbsoluteDevicePathArg) ? saveName : (std::string("save:/") + saveName);
    const bool isSaveDevicePath = savePath.rfind("save:/", 0) == 0;

    if (isSaveDevicePath) {
        LOG_DEBUG("Mounting save data for title ID: " + std::to_string(title->getTitleId()) +
                  " user ID: " + std::to_string(currentUserId.uid[1]) +
                  " save path: " + savePath);

        // Ensure any previously mounted save device is cleanly unmounted first
        fsdevUnmountDevice("save");

        // Mount save data for read-write access
        Result rc = fsdevMountSaveData("save", title->getTitleId(), currentUserId);
        if (R_FAILED(rc)) {
            std::stringstream hexStream;
            hexStream << std::hex << rc;
            LOG_ERROR("Failed to mount save data. Title ID: " + std::to_string(title->getTitleId()) + 
                      " Result: " + std::to_string(rc) + " (0x" + hexStream.str() + ")");
            return nullptr;
        }
        LOG_DEBUG("Successfully mounted save data");
    }

    if (!std::filesystem::exists(savePath)) {
        if (isSaveDevicePath) {
            fsdevUnmountDevice("save");
        }
        LOG_ERROR("Save file does not exist: " + savePath);
        return nullptr;
    }

    try {
        auto& settings = pksm::utils::SettingsManager::getInstance();
        const bool backupEnabled = settings.getBool("backup_save", true);
        if (backupEnabled) {
            const std::string gameName = SanitizeForPathSegment(title->getName());
            const std::string ts = MakeTimestampForPath();

            const std::filesystem::path src_path(savePath);
            const std::string file_name = src_path.filename().string();
            const std::filesystem::path dest_dir(std::string("sdmc:/switch/PKSM/savebackup/") + gameName + "/" + ts);
            const std::filesystem::path dest_path = dest_dir / file_name;

            std::error_code ec;
            std::filesystem::create_directories(dest_dir, ec);
            if (ec) {
                LOG_WARNING("Failed to create backup directory: " + dest_dir.string() + " (" + ec.message() + ")");
            } else {
                if (CopyFileBinary(src_path.string(), dest_path.string())) {
                    LOG_INFO("Backed up save to: " + dest_path.string());
                } else {
                    LOG_WARNING("Save backup copy failed for: " + src_path.string());
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_WARNING("Save backup failed, continuing without backup: " + std::string(e.what()));
    }

    std::uintmax_t reported_size = 0;
    try {
        reported_size = std::filesystem::file_size(savePath);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to query save file size (will read anyway): " + savePath + " (" + std::string(e.what()) + ")");
    }

    std::vector<u8> buffer_vec;
    std::FILE* fp = std::fopen(savePath.c_str(), "rb");
    if (fp == nullptr) {
        const int e = errno;
        if (isSaveDevicePath) {
            fsdevUnmountDevice("save");
        }
        LOG_ERROR("Failed to fopen save file: " + savePath + " (errno " + std::to_string(e) + ": " + std::string(std::strerror(e)) + ")");
        return nullptr;
    }

    long file_size_l = 0;
    if (std::fseek(fp, 0, SEEK_END) == 0) {
        file_size_l = std::ftell(fp);
        (void)std::fseek(fp, 0, SEEK_SET);
    }

    if (file_size_l <= 0) {
        file_size_l = 0;
    }

    try {
        if (file_size_l > 0) {
            buffer_vec.resize(static_cast<size_t>(file_size_l));
            const size_t got = std::fread(buffer_vec.data(), 1, buffer_vec.size(), fp);
            buffer_vec.resize(got);
        } else {
            constexpr size_t kChunk = 0x4000;
            std::array<u8, kChunk> chunk{};
            while (true) {
                const size_t got = std::fread(chunk.data(), 1, chunk.size(), fp);
                if (got == 0) {
                    break;
                }
                const size_t old_sz = buffer_vec.size();
                buffer_vec.resize(old_sz + got);
                std::memcpy(buffer_vec.data() + old_sz, chunk.data(), got);
            }
        }
    } catch (const std::bad_alloc& e) {
        std::fclose(fp);
        if (isSaveDevicePath) {
            fsdevUnmountDevice("save");
        }
        LOG_ERROR("Memory allocation failed while reading save: " + std::string(e.what()));
        return nullptr;
    }

    const int read_errno = errno;
    std::fclose(fp);

    if (buffer_vec.empty()) {
        if (isSaveDevicePath) {
            fsdevUnmountDevice("save");
        }
        LOG_ERROR("Save file read 0 bytes: " + savePath + " (reported: " + std::to_string(reported_size) + ", errno " + std::to_string(read_errno) + ": " + std::string(std::strerror(read_errno)) + ")");
        return nullptr;
    }

    const std::streamsize size = static_cast<std::streamsize>(buffer_vec.size());
    LOG_DEBUG("Read save bytes: " + std::to_string(size) + " (reported: " + std::to_string(reported_size) + ") path: " + savePath);

    std::shared_ptr<u8[]> buffer;
    try {
        buffer = std::shared_ptr<u8[]>(new u8[size], std::default_delete<u8[]>());
        std::copy(buffer_vec.begin(), buffer_vec.end(), buffer.get());
    } catch (const std::bad_alloc& e) {
        LOG_ERROR("Memory allocation failed for PKSM-Core buffer: " + std::string(e.what()));
        return nullptr;
    }

    // keep device mounted for StorageScreen access, don't unmount here

    std::unique_ptr<pksm::Sav> sav;
    try {
        sav = pksm::Sav::getSave(buffer, static_cast<size_t>(size));
    } catch (const std::exception& e) {
        LOG_ERROR("PKSM-Core failed to parse save: " + std::string(e.what()));
        LOG_ERROR("This save file may be corrupted, incompatible, or from an unsupported game.");
        LOG_ERROR("Please try selecting a different save file or check if the game is properly installed.");
        if (isSaveDevicePath) {
            fsdevUnmountDevice("save");
        }
        return nullptr;
    }

    if (!sav) {
        LOG_ERROR("PKSM-Core could not detect a valid save type");
        if (isSaveDevicePath) {
            fsdevUnmountDevice("save");
        }
        return nullptr;
    }

    const auto generation = ToAppGeneration(sav->generation());
    const auto version = ToAppGameVersion(sav->version());
    const auto gender = ToAppGender(sav->gender());

    const u16 tid = sav->TID();
    const u16 sid = sav->SID();
    const u8 badges = sav->badges();

    const int dexSeen = sav->dexSeen();
    const int dexCaught = sav->dexCaught();

    const u16 playedHours = sav->playedHours();
    const u8 playedMinutes = sav->playedMinutes();
    const u8 playedSeconds = sav->playedSeconds();

    LOG_DEBUG("[SaveDataAccessor] Loaded save. Dex seen = " + std::to_string(dexSeen) + ", Dex Caught = " + std::to_string(dexCaught));

    // keep device mounted here aswell for StorageScreen access

    auto save_data = std::make_shared<pksm::saves::SaveData>(
        saveName,
        generation,
        version,
        sav->otName(),
        tid,
        sid,
        gender,
        badges,
        static_cast<u16>(std::max(0, dexSeen)),
        static_cast<u16>(std::max(0, dexCaught)),
        0,
        playedHours,
        playedMinutes,
        playedSeconds
    );

    std::vector<pksm::saves::BagItem> bag_items;
    try {
        const auto pouches = sav->pouches();
        for (const auto& pouch_info : pouches) {
            const auto pouch = pouch_info.first;
            const auto slot_count = pouch_info.second;
            for (int slot = 0; slot < slot_count; slot++) {
                auto item = sav->item(pouch, static_cast<u16>(slot));
                if (!item) {
                    continue;
                }

                const auto id = item->id();
                const auto count = item->count();
                if ((id == 0) || (count == 0)) {
                    continue;
                }

                bag_items.push_back(pksm::saves::BagItem{
                    ToAppBagPouch(pouch),
                    static_cast<u16>(id),
                    static_cast<u16>(count),
                });
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("[SaveDataAccessor] Failed to extract bag items: " + std::string(e.what()));
    }

    save_data->setBagItems(std::move(bag_items));
    return save_data;
}