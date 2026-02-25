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
};