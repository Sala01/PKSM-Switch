#include "data/providers/PartyDataProvider.hpp"

#include <fstream>
#include <vector>
#include <switch.h>

#include "pksmcore/sav/Sav.hpp"
#include "utils/Logger.hpp"

namespace {
std::unique_ptr<pksm::Sav> LoadSavFromPath(const std::string& saveName) {
    const bool isSaveDevicePath = saveName.rfind("save:/", 0) == 0;
    const bool isAbsoluteDevicePath = (saveName.find(":/") != std::string::npos);
    const std::string path = (isSaveDevicePath || isAbsoluteDevicePath) ? saveName : (std::string("save:/") + saveName);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.good()) throw std::runtime_error("open failed");
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<u8> v((size_t)size);
    if (!file.read(reinterpret_cast<char*>(v.data()), size)) throw std::runtime_error("read failed");

    auto buffer = std::shared_ptr<u8[]>(new u8[(size_t)size], std::default_delete<u8[]>());
    std::copy(v.begin(), v.end(), buffer.get());
    auto sav = pksm::Sav::getSave(buffer, (size_t)size);
    if (!sav) throw std::runtime_error("invalid save");
    sav->beginEditing();
    return sav;
}

bool PersistSave(pksm::Sav* sav, const std::string& saveName) {
    if (!sav) return false;
    try {
        sav->finishEditing();
        const bool isSaveDevicePath = saveName.rfind("save:/", 0) == 0;
        const bool isAbsoluteDevicePath = (saveName.find(":/") != std::string::npos);
        const std::string path = (isSaveDevicePath || isAbsoluteDevicePath) ? saveName : (std::string("save:/") + saveName);
        const size_t outSize = static_cast<size_t>(sav->getEntireLengthIncludingFooter());
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.good()) { sav->beginEditing(); return false; }
        file.write(reinterpret_cast<const char*>(sav->rawData().get()), static_cast<std::streamsize>(outSize));
        file.flush();
        file.close();
        Result rc = 0;
        if (isSaveDevicePath) {
            rc = fsdevCommitDevice("save");
        }
        sav->beginEditing();
        if (R_FAILED(rc)) return false;
        return true;
    } catch (...) {
        try { sav->beginEditing(); } catch (...) {}
        return false;
    }
}

pksm::ui::BoxPokemonData PkxToVisual(const pksm::PKX& pk) {
    const u16 form_u16 = pk.alternativeForm();
    const u8 form = form_u16 > 255 ? 0 : static_cast<u8>(form_u16);
    return pksm::ui::BoxPokemonData(static_cast<u16>(pk.species()), form, pk.shiny());
}
} // namespace

pksm::Sav* PartyDataProvider::GetSavForSaveData(const pksm::saves::SaveData::Ref& saveData) const {
    if (!saveData) {
        cachedSav.reset();
        cachedSaveDataPtr = nullptr;
        cachedSaveName.clear();
        saveDirty = false;
        return nullptr;
    }
    if (cachedSav && cachedSaveDataPtr == saveData.get()) return cachedSav.get();

    try {
        cachedSav = LoadSavFromPath(saveData->getName());
        cachedSaveDataPtr = saveData.get();
        cachedSaveName = saveData->getName();
        saveDirty = false;
        return cachedSav.get();
    } catch (const std::exception& e) {
        pksm::utils::Logger::Error(std::string("[PartyDataProvider] load failed: ") + e.what());
        cachedSav.reset();
        cachedSaveDataPtr = nullptr;
        cachedSaveName.clear();
        saveDirty = false;
        return nullptr;
    }
}

size_t PartyDataProvider::GetPartyCount(const pksm::saves::SaveData::Ref& saveData) const {
    auto* sav = GetSavForSaveData(saveData);
    if (!sav) return 0;
    return std::min<size_t>(6, sav->partyCount());
}

pksm::ui::BoxData PartyDataProvider::GetPartyData(const pksm::saves::SaveData::Ref& saveData) const {
    pksm::ui::BoxData team("Team");
    team.resize(6);
    auto* sav = GetSavForSaveData(saveData);
    if (!sav) {
        pksm::utils::Logger::Error("[PartyDataProvider] GetPartyData: no save loaded");
        return team;
    }

    const int reportedCount = static_cast<int>(sav->partyCount());
    // Read all 6 slots regardless — some games report partyCount=0 even
    // when Pokémon are present. We treat any slot with species>0 as filled.
    const int count = std::min<int>(6, std::max(reportedCount, 6));
    pksm::utils::Logger::Debug("[PartyDataProvider] GetPartyData: partyCount=" + std::to_string(reportedCount));
    for (int i = 0; i < count; i++) {
        auto pk = sav->pkm(static_cast<u8>(i));
        if (!pk) continue;
        // Decrypt if still encrypted (matches BoxDataProvider pattern)
        if (pk->isEncrypted()) {
            pk->decrypt();
        }
        const u16 species = static_cast<u16>(pk->species());
        pksm::utils::Logger::Debug("[PartyDataProvider] Party slot " + std::to_string(i) +
                                   " species=" + std::to_string(species));
        if (species == 0) continue;
        team.pokemon[i] = PkxToVisual(*pk);
    }
    return team;
}

std::unique_ptr<pksm::PKX> PartyDataProvider::GetPartyPokemon(const pksm::saves::SaveData::Ref& saveData, int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= 6) return nullptr;
    auto* sav = GetSavForSaveData(saveData);
    if (!sav) return nullptr;
    auto pk = sav->pkm(static_cast<u8>(slotIndex));
    if (pk && pk->isEncrypted()) {
        pk->decrypt();
    }
    return pk;
}

bool PartyDataProvider::WritePartyPokemon(const pksm::saves::SaveData::Ref& saveData, int slotIndex, const pksm::PKX& pkx) {
    if (slotIndex < 0 || slotIndex >= 6) return false;
    auto* sav = GetSavForSaveData(saveData);
    if (!sav) return false;
    sav->pkm(pkx, static_cast<u8>(slotIndex));
    saveDirty = true;
    return true;
}

bool PartyDataProvider::ClearPartySlot(const pksm::saves::SaveData::Ref& saveData, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= 6) return false;
    auto* sav = GetSavForSaveData(saveData);
    if (!sav) return false;
    auto current = sav->pkm(static_cast<u8>(slotIndex));
    if (!current) return false;
    auto blank = current->clone();
    blank->species(pksm::Species::None);
    blank->refreshChecksum();
    sav->pkm(*blank, static_cast<u8>(slotIndex));
    saveDirty = true;
    return true;
}

bool PartyDataProvider::HasPendingWrites() const {
    return saveDirty;
}

bool PartyDataProvider::FlushPendingWrites() {
    if (!saveDirty || !cachedSav) return true;
    if (cachedSaveName.empty()) return false;
    const bool ok = PersistSave(cachedSav.get(), cachedSaveName);
    if (ok) saveDirty = false;
    return ok;
}

void PartyDataProvider::DiscardPendingWrites() {
    if (!saveDirty) return;

    cachedSav.reset();
    cachedSaveDataPtr = nullptr;
    cachedSaveName.clear();
    saveDirty = false;
}