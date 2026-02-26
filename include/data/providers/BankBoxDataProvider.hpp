#pragma once

#include <memory>
#include <string>
#include <vector>

#include "data/providers/interfaces/IBoxDataProvider.hpp"
#include "data/saves/SaveData.hpp"
#include "gui/shared/components/BoxPokemonData.hpp"

namespace pksm
{
    class PKX;
}

class BankBoxDataProvider : public IBoxDataProvider
{
private:
    struct BankEntry;

    std::string bankName;
    std::string rootPath;

    mutable bool loaded;
    mutable int boxCount;
    mutable std::vector<BankEntry> entries;
    mutable std::vector<std::string> boxNames;

    void EnsureLoaded() const;

    // Persist all bank entries to the .bnk file on disk
    bool SaveBank() const;
    // Persist box names to the .json file on disk
    bool SaveNames() const;

public:
    explicit BankBoxDataProvider(std::string bankName = "pksm_1");
    ~BankBoxDataProvider() override;

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

    // Write a full Pokemon to a bank slot (encrypts and persists to disk)
    bool WritePokemon(int boxIndex, int slotIndex, const pksm::PKX& pkx) override;
    // Clear a bank slot, marking it as empty
    bool ClearSlot(int boxIndex, int slotIndex) override;
    // Update a box name and persist to disk
    bool SetBoxName(int boxIndex, const std::string& name);
};