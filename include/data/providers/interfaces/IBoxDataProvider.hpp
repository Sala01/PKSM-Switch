#pragma once

#include <memory>
#include <vector>

#include "data/saves/SaveData.hpp"
#include "gui/shared/components/BoxPokemonData.hpp"
#include "pksmcore/pkx/PKX.hpp"

class IBoxDataProvider {
public:
    PU_SMART_CTOR(IBoxDataProvider)
    virtual ~IBoxDataProvider() = default;

    // Get the number of boxes available for a save
    virtual size_t GetBoxCount(const pksm::saves::SaveData::Ref& saveData) const = 0;

    // Get the data for a specific box
    virtual pksm::ui::BoxData GetBoxData(const pksm::saves::SaveData::Ref& saveData, int boxIndex) const = 0;

    // Set data for a specific box (returns success/failure)
    virtual bool
    SetBoxData(const pksm::saves::SaveData::Ref& saveData, int boxIndex, const pksm::ui::BoxData& boxData) = 0;

    // Set data for a specific Pokémon in a box (returns success/failure)
    virtual bool SetPokemonData(
        const pksm::saves::SaveData::Ref& saveData,
        int boxIndex,
        int slotIndex,
        const pksm::ui::BoxPokemonData& pokemonData
    ) = 0;

    virtual std::unique_ptr<pksm::PKX> GetPokemon(
        const pksm::saves::SaveData::Ref& saveData,
        int boxIndex,
        int slotIndex
    ) const = 0;

    // Write a full Pokemon to a specific box slot.
    // Unlike SetPokemonData (which only has species/form/shiny), this accepts
    // the complete PKX data needed for persistence.
    virtual bool WritePokemon(int boxIndex, int slotIndex, const pksm::PKX& pkx) {
        (void)boxIndex; (void)slotIndex; (void)pkx;
        return false;
    }

    // Clear a specific box slot, marking it as empty.
    virtual bool ClearSlot(int boxIndex, int slotIndex) {
        (void)boxIndex; (void)slotIndex;
        return false;
    }
};