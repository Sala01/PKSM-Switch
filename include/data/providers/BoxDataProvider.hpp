#pragma once

#include <memory>
#include <string>
#include <vector>

#include "data/providers/interfaces/IBoxDataProvider.hpp"
#include "data/saves/SaveData.hpp"
#include "gui/shared/components/BoxPokemonData.hpp"

namespace pksm
{
    class Sav;
}

class BoxDataProvider : public IBoxDataProvider
{
private:
    mutable const pksm::saves::SaveData *cachedSaveDataPtr = nullptr;
    mutable std::unique_ptr<pksm::Sav> cachedSav;

    pksm::Sav *GetSavForSaveData(const pksm::saves::SaveData::Ref &saveData) const;

    // load box data from save file
    pksm::ui::BoxData LoadBoxDataFromSave(
        const pksm::saves::SaveData::Ref &saveData,
        int boxIndex) const;

    // Persist the cached save to disk (finishEditing → write → commit → beginEditing)
    bool PersistSave(pksm::Sav *sav, const std::string &saveName) const;

public:
    BoxDataProvider();
    ~BoxDataProvider() override;

    // IBoxDataProvider interface implementation
    size_t GetBoxCount(const pksm::saves::SaveData::Ref &saveData) const override;
    pksm::ui::BoxData GetBoxData(const pksm::saves::SaveData::Ref &saveData, int boxIndex) const override;
    bool SetBoxData(
        const pksm::saves::SaveData::Ref &saveData,
        int boxIndex,
        const pksm::ui::BoxData &boxData) override;
    bool SetPokemonData(
        const pksm::saves::SaveData::Ref &saveData,
        int boxIndex,
        int slotIndex,
        const pksm::ui::BoxPokemonData &pokemonData) override;

    std::unique_ptr<pksm::PKX> GetPokemon(
        const pksm::saves::SaveData::Ref &saveData,
        int boxIndex,
        int slotIndex) const override;

    // Write a full Pokemon to a save box slot and persist to disk
    bool WritePokemon(
        const pksm::saves::SaveData::Ref &saveData,
        int boxIndex, int slotIndex, const pksm::PKX &pkx) override;

    // Clear a save box slot and persist to disk
    bool ClearSlot(
        const pksm::saves::SaveData::Ref &saveData,
        int boxIndex, int slotIndex) override;

    // Convert PKX to the native format for this save before writing
    std::unique_ptr<pksm::PKX> PrepareForWrite(
        const pksm::saves::SaveData::Ref &saveData,
        const pksm::PKX &pkx) override;
};
