#include "data/providers/BoxDataProvider.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <switch.h>

#include "pksmcore/pkx/PA8.hpp"
#include "pksmcore/pkx/PK8.hpp"
#include "pksmcore/pkx/PKX.hpp"
#include "pksmcore/sav/Sav.hpp"
#include "pksmcore/utils/VersionTables.hpp"
#include "utils/Logger.hpp"

namespace {

std::unique_ptr<pksm::Sav> LoadSavFromPath(const std::string &save_name) {
    const bool isSaveDevicePath = save_name.rfind("save:/", 0) == 0;
    const bool isAbsoluteDevicePath = (save_name.find(":/") != std::string::npos);
    const std::string savePath = (isSaveDevicePath || isAbsoluteDevicePath) ? save_name : (std::string("save:/") + save_name);

    if (!std::filesystem::exists(savePath)) {
        throw std::runtime_error("Save file does not exist: " + savePath);
    }

    std::ifstream in(savePath, std::ios::binary | std::ios::ate);
    if (!in.good()) {
        throw std::runtime_error("Failed to open save file: " + savePath);
    }

    const std::streamsize size = in.tellg();
    if (size <= 0) {
        throw std::runtime_error("Invalid save file size: " + savePath);
    }

    in.seekg(0, std::ios::beg);

    std::vector<u8> buffer_vec;
    buffer_vec.resize(size);
    if (!in.read(reinterpret_cast<char*>(buffer_vec.data()), size)) {
        throw std::runtime_error("Failed to read save file: " + savePath);
    }

    auto buffer = std::shared_ptr<u8[]>(new u8[size], std::default_delete<u8[]>());
    std::copy(buffer_vec.begin(), buffer_vec.end(), buffer.get());

    auto sav = pksm::Sav::getSave(buffer, static_cast<size_t>(size));
    if (!sav) {
        throw std::runtime_error("PKSM-Core could not detect a valid save type");
    }

    return sav;
}

// Convert PK8 → PA8 field-by-field for PLA saves.
// convertToG8() returns std::unique_ptr<PK8> and cannot return PA8 (different layout/size),
// so this helper is used at the provider level when the target save is PLA.
std::unique_ptr<pksm::PKX> ConvertPK8toPA8(const pksm::PK8& pk8) {
    auto pa8 = pksm::PKX::getPKM<pksm::PA8>(nullptr, pksm::PA8::BOX_LENGTH);

    pa8->encryptionConstant(pk8.encryptionConstant());
    pa8->species(pk8.species());
    pa8->TID(pk8.TID());
    pa8->SID(pk8.SID());
    pa8->experience(pk8.experience());
    pa8->PID(pk8.PID());

    if (pk8.ability() == pksm::PersonalSWSH::ability(pk8.formSpecies(), pk8.abilityNumber() >> 1))
    {
        pa8->setAbility(pk8.abilityNumber() >> 1);
    }
    else
    {
        pa8->ability(pk8.ability());
        pa8->abilityNumber(pk8.abilityNumber());
    }

    pa8->language(pk8.language());
    pa8->heldItem(pk8.heldItem());
    pa8->markValue(pk8.markValue());

    for (pksm::Stat stat : {pksm::Stat::HP, pksm::Stat::ATK, pksm::Stat::DEF,
                            pksm::Stat::SPATK, pksm::Stat::SPDEF, pksm::Stat::SPD})
    {
        pa8->ev(stat, pk8.ev(stat));
        pa8->iv(stat, pk8.iv(stat));
        pa8->hyperTrain(stat, pk8.hyperTrain(stat));
    }

    for (size_t i = 0; i < 4; i++)
    {
        pa8->move(i, pk8.move(i));
        pa8->PPUp(i, pk8.PPUp(i));
        pa8->PP(i, pk8.PP(i));
        pa8->relearnMove(i, pk8.relearnMove(i));
    }

    pa8->egg(pk8.egg());
    pa8->nicknamed(pk8.nicknamed());
    pa8->nickname(pk8.nickname());
    pa8->fatefulEncounter(pk8.fatefulEncounter());
    pa8->gender(pk8.gender());
    pa8->otGender(pk8.otGender());
    pa8->alternativeForm(pk8.alternativeForm());
    pa8->nature(pk8.nature());

    pa8->version(pk8.version());
    pa8->otName(pk8.otName());
    pa8->otFriendship(pk8.otFriendship());
    pa8->metDate(pk8.metDate());
    pa8->eggDate(pk8.eggDate());
    pa8->metLocation(pk8.metLocation());
    pa8->eggLocation(pk8.eggLocation());
    pa8->ball(pk8.ball());
    pa8->metLevel(pk8.metLevel());
    pa8->currentHandler(pk8.currentHandler());
    pa8->htFriendship(pk8.htFriendship());

    pa8->pkrsStrain(pk8.pkrsStrain());
    pa8->pkrsDays(pk8.pkrsDays());

    for (size_t i = 0; i < 6; i++)
    {
        pa8->contest(i, pk8.contest(i));
    }

    // PA8 doesn't use ribbons (PLA has no ribbon system)

    pa8->refreshChecksum();
    return pa8;
}

// Clear moves not supported in the target game version, matching PKHeX ClearInvalidMoves().
void ClearInvalidMoves(pksm::PKX& pk, pksm::GameVersion targetVersion) {
    const auto& validMoves = pksm::VersionTables::availableMoves(targetVersion);
    if (validMoves.empty()) return;

    for (int i = 0; i < 4; i++)
    {
        if (pk.move(i) != pksm::Move::None && validMoves.count(pk.move(i)) == 0)
        {
            pk.move(i, pksm::Move::None);
            pk.PP(i, 0);
            pk.PPUp(i, 0);
        }
        if (pk.relearnMove(i) != pksm::Move::None && validMoves.count(pk.relearnMove(i)) == 0)
        {
            pk.relearnMove(i, pksm::Move::None);
        }
    }
    pk.fixMoves();
}

// INTL Gen I/II hold 20 Pokemon per box while JPN Gen I/II hold 30
bool saveIsTwentySlot(const pksm::Sav* sav) {
    if (!sav) {
        return false;
    }
    const int boxes = sav->maxBoxes();
    if (boxes <= 0) {
        return false;
    }
    return (sav->maxSlot() / boxes) == 20;
}

bool paddingSlot(int gridSlot) {
    if (gridSlot < 0 || gridSlot >= 30) {
        return true;
    }
    const int col = gridSlot % 6;
    return col == 0 || col == 5;
}

int toSaveSlot(int gridSlot) {
    const int row = gridSlot / 6;
    const int col = gridSlot % 6;
    if (col < 1 || col > 4) {
        return -1;
    }
    return row * 4 + (col - 1);
}

int toGridSlot(int saveSlot) {
    return (saveSlot / 4) * 6 + 1 + (saveSlot % 4);
}

} // namespace

BoxDataProvider::BoxDataProvider() = default;

BoxDataProvider::~BoxDataProvider() = default;

pksm::Sav* BoxDataProvider::GetSavForSaveData(const pksm::saves::SaveData::Ref& saveData) const {
    if (!saveData) {
        cachedSav.reset();
        cachedSaveDataPtr = nullptr;
        cachedSaveName.clear();
        saveDirty = false;
        return nullptr;
    }

    const auto *save_ptr = saveData.get();
    if (cachedSav && (cachedSaveDataPtr == save_ptr)) {
        return cachedSav.get();
    }

    try {
        cachedSav = LoadSavFromPath(saveData->getName());
        cachedSaveDataPtr = save_ptr;
        cachedSaveName = saveData->getName();
        saveDirty = false;
        return cachedSav.get();
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BoxDataProvider] Failed to cache save: ") + e.what());
        cachedSav.reset();
        cachedSaveDataPtr = nullptr;
        cachedSaveName.clear();
        saveDirty = false;
        return nullptr;
    }
}

size_t BoxDataProvider::GetBoxCount(const pksm::saves::SaveData::Ref& saveData) const {
    if (!saveData) {
        return 0;
    }

    try {
        auto *sav = GetSavForSaveData(saveData);
        if (!sav) {
            return 0;
        }
        const int count = sav->maxBoxes();
        return count > 0 ? static_cast<size_t>(count) : 0;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BoxDataProvider] GetBoxCount failed: ") + e.what());
        return 0;
    }
}

pksm::ui::BoxData BoxDataProvider::GetBoxData(const pksm::saves::SaveData::Ref& saveData, int boxIndex) const {
    return LoadBoxDataFromSave(saveData, boxIndex);
}

bool BoxDataProvider::SetBoxData(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex,
    const pksm::ui::BoxData& boxData
) {
    // BoxData only carries species/form/shiny per slot — not enough for full persistence.
    // We can update the box name if the save supports it.
    if (!saveData) return false;

    auto *sav = GetSavForSaveData(saveData);
    if (!sav) return false;
    if (boxIndex < 0 || boxIndex >= sav->maxBoxes()) return false;

    sav->boxName(static_cast<u8>(boxIndex), boxData.name);
    saveDirty = true;
    return true;
}

bool BoxDataProvider::SetPokemonData(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex,
    int slotIndex,
    const pksm::ui::BoxPokemonData& pokemonData
) {
    // BoxPokemonData only has species/form/shiny — not enough to write a full Pokemon.
    // We can handle the "clear slot" case (species == 0).
    if (pokemonData.isEmpty()) {
        return ClearSlot(saveData, boxIndex, slotIndex);
    }
    pksm::utils::Logger::Error("[BoxDataProvider] SetPokemonData: use WritePokemon() for full PKX writes");
    return false;
}

std::unique_ptr<pksm::PKX> BoxDataProvider::GetPokemon(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex,
    int slotIndex
) const {
    if (!saveData) {
        return nullptr;
    }

    if ((boxIndex < 0) || (slotIndex < 0) || (slotIndex >= 30)) {
        return nullptr;
    }

    try {
        auto *sav = GetSavForSaveData(saveData);
        if (!sav) {
            return nullptr;
        }

        const int max_boxes = sav->maxBoxes();
        if ((boxIndex < 0) || (boxIndex >= max_boxes)) {
            return nullptr;
        }

        int readSlot = slotIndex;
        if (saveIsTwentySlot(sav)) {
            if (paddingSlot(slotIndex)) {
                return nullptr;
            }
            readSlot = toSaveSlot(slotIndex);
            if (readSlot < 0 || readSlot >= 20) {
                return nullptr;
            }
        }

        auto pk = sav->pkm(static_cast<u8>(boxIndex), static_cast<u8>(readSlot));
        if (!pk) {
            return nullptr;
        }

        if (pk->isEncrypted()) {
            pk->decrypt();
        }

        if (static_cast<u16>(pk->species()) == 0) {
            return nullptr;
        }

        return pk;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BoxDataProvider] GetPokemon failed: ") + e.what());
        return nullptr;
    }
}

pksm::ui::BoxData BoxDataProvider::LoadBoxDataFromSave(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex
) const {
    pksm::ui::BoxData boxData;

    if (!saveData) {
        boxData.name = "Box";
        boxData.resize(30);
        return boxData;
    }

    try {
        auto *sav = GetSavForSaveData(saveData);
        if (!sav) {
            boxData.name = "Box";
            boxData.resize(30);
            return boxData;
        }

        const int max_boxes = sav->maxBoxes();
        if ((boxIndex < 0) || (boxIndex >= max_boxes)) {
            boxData.name = "Box";
            boxData.resize(30);
            return boxData;
        }

        boxData.name = sav->boxName(static_cast<u8>(boxIndex));

        // normalize box names (some games may return empty strings by design, like LGPE)
        boxData.name.erase(std::remove(boxData.name.begin(), boxData.name.end(), '\0'), boxData.name.end());
        const bool nameAllWhitespace = std::all_of(boxData.name.begin(), boxData.name.end(), [](unsigned char c) {
            return std::isspace(c) != 0;
        });
        if (boxData.name.empty() || nameAllWhitespace) {
            boxData.name = "Box " + std::to_string(boxIndex + 1);
        }

        boxData.resize(30);

        if (saveIsTwentySlot(sav)) {
            for (int s = 0; s < 20; s++) {
                const int g = toGridSlot(s);
                auto pk = sav->pkm(static_cast<u8>(boxIndex), static_cast<u8>(s));
                if (!pk) {
                    boxData.pokemon[g] = pksm::ui::BoxPokemonData();
                    continue;
                }
                if (pk->isEncrypted()) {
                    pk->decrypt();
                }
                const u16 species = static_cast<u16>(pk->species());
                if (species == 0) {
                    boxData.pokemon[g] = pksm::ui::BoxPokemonData();
                    continue;
                }
                const u16 form_u16 = pk->alternativeForm();
                const u8 form = form_u16 > 255 ? 0 : static_cast<u8>(form_u16);
                boxData.pokemon[g] = pksm::ui::BoxPokemonData(species, form, pk->shiny());
            }
            for (int g = 0; g < 30; g++) {
                if (paddingSlot(g)) {
                    boxData.pokemon[g] = pksm::ui::BoxPokemonData();
                }
            }
        } else {
            for (int slot = 0; slot < 30; slot++) {
                auto pk = sav->pkm(static_cast<u8>(boxIndex), static_cast<u8>(slot));
                if (!pk) {
                    boxData.pokemon[slot] = pksm::ui::BoxPokemonData();
                    continue;
                }

                if (pk->isEncrypted()) {
                    pk->decrypt();
                }

                const u16 species = static_cast<u16>(pk->species());
                if (species == 0) {
                    boxData.pokemon[slot] = pksm::ui::BoxPokemonData();
                    continue;
                }

                const u16 form_u16 = pk->alternativeForm();
                const u8 form = form_u16 > 255 ? 0 : static_cast<u8>(form_u16);
                const bool shiny = pk->shiny();

                boxData.pokemon[slot] = pksm::ui::BoxPokemonData(species, form, shiny);
            }
        }

        return boxData;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BoxDataProvider] LoadBoxDataFromSave failed: ") + e.what());
        boxData.name = "Box";
        boxData.resize(30);
        return boxData;
    }
}

bool BoxDataProvider::WritePokemon(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex, int slotIndex, const pksm::PKX& pkx
) {
    if (!saveData) return false;

    try {
        auto *sav = GetSavForSaveData(saveData);
        if (!sav) return false;
        if (boxIndex < 0 || boxIndex >= sav->maxBoxes() || slotIndex < 0 || slotIndex >= 30)
            return false;

        int writeSlot = slotIndex;
        if (saveIsTwentySlot(sav)) {
            if (paddingSlot(slotIndex)) {
                return false;
            }
            writeSlot = toSaveSlot(slotIndex);
            if (writeSlot < 0 || writeSlot >= 20) {
                return false;
            }
        }

        // sav->pkm() setter expects a decrypted PKX — the save handles internal encryption
        sav->pkm(pkx, static_cast<u8>(boxIndex), static_cast<u8>(writeSlot), false);
        saveDirty = true;
        return true;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BoxDataProvider] WritePokemon failed: ") + e.what());
        return false;
    }
}

bool BoxDataProvider::ClearSlot(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex, int slotIndex
) {
    if (!saveData) return false;

    try {
        auto *sav = GetSavForSaveData(saveData);
        if (!sav) return false;
        if (boxIndex < 0 || boxIndex >= sav->maxBoxes() || slotIndex < 0 || slotIndex >= 30)
            return false;

        if (saveIsTwentySlot(sav) && paddingSlot(slotIndex)) {
            return true;
        }

        int clearSlot = slotIndex;
        if (saveIsTwentySlot(sav)) {
            clearSlot = toSaveSlot(slotIndex);
            if (clearSlot < 0 || clearSlot >= 20) {
                return false;
            }
        }

        // Read the existing PKX to get the right type/size for this save, then zero it out.
        // A zeroed PKX has species==0 which the read path treats as empty.
        auto pk = sav->pkm(static_cast<u8>(boxIndex), static_cast<u8>(clearSlot));
        if (pk) {
            std::fill_n(pk->rawData().data(), pk->getLength(), u8(0));
            sav->pkm(*pk, static_cast<u8>(boxIndex), static_cast<u8>(clearSlot), false);
        }

        saveDirty = true;
        return true;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BoxDataProvider] ClearSlot failed: ") + e.what());
        return false;
    }
}

bool BoxDataProvider::HasPendingWrites() const {
    return saveDirty;
}

bool BoxDataProvider::FlushPendingWrites() const {
    if (!saveDirty) {
        return true;
    }

    if (!cachedSav || cachedSaveName.empty()) {
        return false;
    }

    if (!PersistSave(cachedSav.get(), cachedSaveName)) {
        return false;
    }

    saveDirty = false;
    return true;
}

void BoxDataProvider::DiscardPendingWrites() const {
    if (!saveDirty) {
        return;
    }

    cachedSav.reset();
    cachedSaveDataPtr = nullptr;
    cachedSaveName.clear();
    saveDirty = false;
}

bool BoxDataProvider::UsesTwentySlotBox(const pksm::saves::SaveData::Ref& saveData) const {
    if (!saveData) {
        return false;
    }
    return saveIsTwentySlot(GetSavForSaveData(saveData));
}

bool BoxDataProvider::PersistSave(pksm::Sav* sav, const std::string& saveName) const {
    // Match LoadSavFromPath: save:/ mount, or full device path (sdmc:/ for emulator saves)
    try {
        sav->finishEditing();

        const bool isSaveDevicePath = saveName.rfind("save:/", 0) == 0;
        const bool isAbsoluteDevicePath = (saveName.find(":/") != std::string::npos);
        const std::string savePath =
            (isSaveDevicePath || isAbsoluteDevicePath) ? saveName : (std::string("save:/") + saveName);

        const size_t outSize = static_cast<size_t>(sav->getEntireLengthIncludingFooter());

        std::ofstream out(savePath, std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            pksm::utils::Logger::Error("[BoxDataProvider] Failed to open save for writing: " + savePath);
            sav->beginEditing();
            return false;
        }

        out.write(reinterpret_cast<const char*>(sav->rawData().get()),
                  static_cast<std::streamsize>(outSize));
        out.flush();
        out.close();

        Result rc = 0;
        if (isSaveDevicePath) {
            rc = fsdevCommitDevice("save");
        }
        sav->beginEditing();

        if (R_FAILED(rc)) {
            pksm::utils::Logger::Error("[BoxDataProvider] Failed to commit save device");
            return false;
        }

        return true;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BoxDataProvider] PersistSave failed: ") + e.what());
        // Ensure save is back in editing mode even on failure
        try { sav->beginEditing(); } catch (...) {}
        return false;
    }
}

std::unique_ptr<pksm::PKX> BoxDataProvider::PrepareForWrite(
    const pksm::saves::SaveData::Ref& saveData,
    const pksm::PKX& pkx)
{
    auto* sav = GetSavForSaveData(saveData);
    if (!sav) return nullptr;

    // Reject transfer if the species doesn't exist in the target game
    const auto& validSpecies = sav->availableSpecies();
    if (!validSpecies.empty() && validSpecies.count(pkx.species()) == 0)
    {
        pksm::utils::Logger::Error("[BoxDataProvider] Transfer rejected: species " +
            std::to_string(u16(pkx.species())) + " not available in target game");
        return nullptr;
    }

    std::unique_ptr<pksm::PKX> result;

    // PLA special case: convertToG8() returns std::unique_ptr<PK8> (can't return PA8
    // due to the fixed return type), so we detect PLA target and post-convert PK8→PA8.
    if (sav->version() == pksm::GameVersion::PLA)
    {
        if (pkx.extension() == ".pa8")
        {
            result = pkx.clone();
        }
        else
        {
            auto pk8 = pkx.convertToG8(*sav);
            if (!pk8) return nullptr;
            result = ConvertPK8toPA8(*pk8);
        }
    }
    else
    {
        // All other saves: Sav::transfer() dispatches to the correct convertToGX()
        result = sav->transfer(pkx);
    }

    // Clear moves not supported in the target game (like PKHeX's ClearInvalidMoves)
    if (result)
    {
        ClearInvalidMoves(*result, sav->version());
        result->refreshChecksum();
    }

    return result;
}