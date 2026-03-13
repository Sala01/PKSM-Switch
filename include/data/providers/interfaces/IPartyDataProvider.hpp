#pragma once

#include <memory>

#include "data/saves/SaveData.hpp"
#include "gui/shared/components/BoxPokemonData.hpp"
#include "pksmcore/pkx/PKX.hpp"

class IPartyDataProvider {
public:
    PU_SMART_CTOR(IPartyDataProvider)
    virtual ~IPartyDataProvider() = default;

    virtual size_t GetPartyCount(const pksm::saves::SaveData::Ref& saveData) const = 0;
    virtual pksm::ui::BoxData GetPartyData(const pksm::saves::SaveData::Ref& saveData) const = 0;
    virtual std::unique_ptr<pksm::PKX> GetPartyPokemon(const pksm::saves::SaveData::Ref& saveData, int slotIndex) const = 0;
    virtual bool WritePartyPokemon(const pksm::saves::SaveData::Ref& saveData, int slotIndex, const pksm::PKX& pkx) = 0;
    virtual bool ClearPartySlot(const pksm::saves::SaveData::Ref& saveData, int slotIndex) = 0;
    virtual bool HasPendingWrites() const { return false; }
    virtual bool FlushPendingWrites() = 0;
    virtual void DiscardPendingWrites() {}
};
