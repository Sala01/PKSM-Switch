#include "TitleDataProvider.hpp"
#include "data/providers/CustomTitleProvider.hpp"
#include <fstream>
#include <filesystem>
#include <set>
#include <vector>
#include <cstring>

#include <stdio.h>

#include <switch.h>

#include <nlohmann/json.hpp>

#include "data/emulator/EmulatorGameCatalog.hpp"
#include "data/emulator/EmulatorSaveConfig.hpp"

namespace pksm::titles {

static constexpr const char* JSON_PATH = "romfs:/gfx/data/data.json";

namespace {

bool PathExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec) && !ec;
}

bool AnyPathExists(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        if (PathExists(p)) {
            return true;
        }
    }
    return false;
}

bool AnyConfiguredPathExists(const pksm::data::emulator::EmulatorSaveSelection& sel) {
    if (AnyPathExists(sel.primary)) {
        return true;
    }
    for (const auto& kv : sel.extra) {
        if (AnyPathExists(kv.second)) {
            return true;
        }
    }
    return false;
}

}

unsigned long long TitleDataProvider::GetInsertedGameCardID() const {
    Result rc = ncmInitialize();
    if (R_FAILED(rc)) {
        return 0;
    }

    NcmContentMetaDatabase ncm_db{};
    unsigned long long inserted_id = 0;
    bool opened_db = false;

    rc = ncmOpenContentMetaDatabase(&ncm_db, NcmStorageId_GameCard);
    if (R_SUCCEEDED(rc)) {
        opened_db = true;
        u32 total = 0;
        u32 written = 0;
        NcmContentMetaKey key{};

        // first call: request at least one entry; meta_type=0 means all.
        rc = ncmContentMetaDatabaseList(&ncm_db, (s32*)&total, (s32*)&written, &key, 1, (NcmContentMetaType)0, 0, 0, UINT64_MAX, NcmContentInstallType_Full);
        if (R_SUCCEEDED(rc) && written > 0) {
            // if the first entry isn't an application, enumerate all keys and pick the first application.
            if (key.type == NcmContentMetaType_Application) {
                inserted_id = key.id;
            } else if (total > 0) {
                auto keys = static_cast<NcmContentMetaKey*>(std::calloc(total, sizeof(NcmContentMetaKey)));
                if (keys) {
                    u32 written2 = 0;
                    rc = ncmContentMetaDatabaseList(&ncm_db, (s32*)&total, (s32*)&written2, keys, (s32)total, (NcmContentMetaType)0, 0, 0, UINT64_MAX, NcmContentInstallType_Full);
                    if (R_SUCCEEDED(rc) && written2 > 0) {
                        for (u32 i = 0; i < written2; i++) {
                            if (keys[i].type == NcmContentMetaType_Application && keys[i].id != 0) {
                                inserted_id = keys[i].id;
                                break;
                            }
                        }
                    }
                    std::free(keys);
                }
            }
        }
    }

    if (opened_db) {
        ncmContentMetaDatabaseClose(&ncm_db);
    }

    ncmExit();
    return inserted_id;
}

void TitleDataProvider::GetInstalledApplicationIds(std::set<u64>& out_ids) const {
    Result rc = nsInitialize();
    if (R_FAILED(rc)) return;

    constexpr size_t MAX_ENTRIES = 0x400; // 1024 entries
    constexpr size_t BUFFER_SIZE = (sizeof(NsApplicationRecord) * MAX_ENTRIES);

    auto records = reinterpret_cast<NsApplicationRecord*>(std::calloc(1, BUFFER_SIZE));
    if (!records) {
        nsExit();
        return;
    }

    s32 count = 0;

    rc = nsListApplicationRecord(records, MAX_ENTRIES, 0, &count);
    if (R_SUCCEEDED(rc)) {
        out_ids.clear();
        for (s32 i = 0; i < count; i++) {
           NsApplicationRecord* cur_record = &(records[i]);
           if (cur_record->application_id) out_ids.insert(cur_record->application_id);
        }
    }

    // Cleanup

    std::free(records);
    nsExit();
}

TitleDataProvider::TitleDataProvider()
{
    customTitleProvider = CustomTitleProvider::New();
    try {
        std::ifstream f(JSON_PATH);
        if (f.is_open()) {
            nlohmann::json j;
            f >> j;

            // load all console titles from JSON
            for (const auto& game : j["games"]) {
                if (game["category"] != "console") continue;

                u64 titleId = 0;
                try { titleId = std::stoull(game["title_id"].get<std::string>(), nullptr, 16); }
                catch (...) { continue; }

                std::string name = game["name"].get<std::string>();
                std::string iconPath = game["icon_path"].get<std::string>();

                installedTitles.push_back(std::make_shared<Title>(name, iconPath, titleId));
            }
        }
    } catch (...) {
        installedTitles.clear();
    }

    try {
        emulatorTitles.clear();

        const auto games = pksm::data::emulator::EmulatorGameCatalog::LoadFromDataJson(JSON_PATH);
        const auto saveCfg = pksm::data::emulator::EmulatorSaveConfig::Load();

        for (const auto& g : games) {
            if (g.titleId == 0) {
                continue;
            }

            bool anySavePresent = AnyPathExists(g.saveProbes);
            if (!anySavePresent) {
                const auto it = saveCfg.find(g.titleId);
                if (it != saveCfg.end()) {
                    anySavePresent = AnyConfiguredPathExists(it->second);
                }
            }

            if (anySavePresent) {
                emulatorTitles.push_back(std::make_shared<Title>(g.name, g.iconPath, g.titleId));
            }
        }
    } catch (...) {
        emulatorTitles.clear();
    }
}

Title::Ref TitleDataProvider::GetGameCardTitle() const {
    const auto insertedId = static_cast<u64>(GetInsertedGameCardID());
    if (insertedId == 0) {
        return nullptr;
    }

    for (const auto& t : installedTitles) {
        if (!t) continue;

        if (t->getTitleId() == insertedId) {
            return t;
        }
    }

    return nullptr;
}

std::vector<Title::Ref> TitleDataProvider::GetInstalledTitles(const AccountUid& /*userId*/) const {

    std::set<u64> installedIds;
    GetInstalledApplicationIds(installedIds);

    std::vector<std::shared_ptr<Title>> filteredList;
    filteredList.reserve(installedTitles.size());

    const auto insertedId = static_cast<u64>(GetInsertedGameCardID());
    for (const auto& t : installedTitles) {
        if (!t) continue;

        if (insertedId != 0 && t->getTitleId() == insertedId) {
            continue;
        }

        if (installedIds.find(t->getTitleId()) != installedIds.end()) {
            filteredList.push_back(t);
        }
    }

    // return the filtered list of installed titles only
    return filteredList;
}

std::vector<Title::Ref> TitleDataProvider::GetEmulatorTitles() const {
    return emulatorTitles;
}

std::vector<Title::Ref> TitleDataProvider::GetCustomTitles() const {
    if (!customTitleProvider) {
        return {};
    }
    return customTitleProvider->GetCustomTitles();
}

}