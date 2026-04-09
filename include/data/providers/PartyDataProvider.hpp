#pragma once

#include "data/providers/interfaces/IPartyDataProvider.hpp"
#include "pksmcore/sav/Sav.hpp"

class PartyDataProvider : public IPartyDataProvider
{
private:
    mutable const pksm::saves::SaveData *cachedSaveDataPtr = nullptr;
    mutable std::unique_ptr<pksm::Sav> cachedSav;
    mutable bool saveDirty = false;
    mutable std::string cachedSaveName;

    pksm::Sav *GetSavForSaveData(const pksm::saves::SaveData::Ref &saveData) const;

public:
    PartyDataProvider() = default;
    ~PartyDataProvider() override = default;

    size_t GetPartyCount(const pksm::saves::SaveData::Ref &saveData) const override;
    pksm::ui::BoxData GetPartyData(const pksm::saves::SaveData::Ref &saveData) const override;
    std::unique_ptr<pksm::PKX> GetPartyPokemon(const pksm::saves::SaveData::Ref &saveData, int slotIndex) const override;
    bool WritePartyPokemon(const pksm::saves::SaveData::Ref &saveData, int slotIndex, const pksm::PKX &pkx) override;
    bool ClearPartySlot(const pksm::saves::SaveData::Ref &saveData, int slotIndex) override;
    bool HasPendingWrites() const override;
    bool FlushPendingWrites() override;
    void DiscardPendingWrites() override;
};