/*
 *   This file is part of PKSM-Core
 *   Copyright (C) 2016-2022 Bernardo Giordano, Admiral Fish, piepie62
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *       * Requiring preservation of specified reasonable legal notices or
 *         author attributions in that material or in the Appropriate Legal
 *         Notices displayed by works containing it.
 *       * Prohibiting misrepresentation of the origin of that material,
 *         or requiring that modified versions of such material be marked in
 *         reasonable ways as different from the original version.
 */

#include "sav/SavSV.hpp"
#include "pkx/PK9.hpp"
#include "sav/Item.hpp"
#include "utils/endian.hpp"
#include "utils/i18n.hpp"
#include "utils/utils.hpp"
#include <algorithm>
#include <cstring>
#include <ranges>
#include <stdexcept>

namespace
{
    // Box name: 0x22 bytes per box, 17 chars UTF-16LE (same as SWSH/ZA)
    constexpr int BOX_NAME_BYTES = 0x22;
    constexpr int BOX_NAME_CHARS = BOX_NAME_BYTES / 2;
}

namespace pksm
{
    SavSV::SavSV(const std::shared_ptr<u8[]>& dt, size_t length) : Sav8(dt, length)
    {
        // Validate this is actually an SV save by checking the version byte
        if (blocks.empty())
        {
            throw std::invalid_argument("Not a valid SV save: no blocks");
        }
        auto statusBlock = getBlock(KStatus);
        if (!statusBlock)
        {
            throw std::invalid_argument("Not a valid SV save: no Status block");
        }
        u8 ver = statusBlock->decryptedData()[0x04];
        if (ver != u8(GameVersion::SL) && ver != u8(GameVersion::VL))
        {
            throw std::invalid_argument("Not a valid SV save: wrong version");
        }

        game      = Game::SLVL;
        Box       = KBox;
        Party     = KParty;
        Status    = KStatus;
        Items     = KItems;
        BoxLayout = KBoxLayout;
    }

    u16 SavSV::TID(void) const
    {
        return LittleEndian::convertTo<u16>(getBlock(Status)->decryptedData() + 0x00);
    }

    void SavSV::TID(u16 v)
    {
        LittleEndian::convertFrom<u16>(getBlock(Status)->decryptedData() + 0x00, v);
    }

    u16 SavSV::SID(void) const
    {
        return LittleEndian::convertTo<u16>(getBlock(Status)->decryptedData() + 0x02);
    }

    void SavSV::SID(u16 v)
    {
        LittleEndian::convertFrom<u16>(getBlock(Status)->decryptedData() + 0x02, v);
    }

    GameVersion SavSV::version(void) const
    {
        return GameVersion(getBlock(Status)->decryptedData()[0x04]);
    }

    void SavSV::version(GameVersion v)
    {
        getBlock(Status)->decryptedData()[0x04] = u8(v);
    }

    Gender SavSV::gender(void) const
    {
        return Gender{getBlock(Status)->decryptedData()[0x05]};
    }

    void SavSV::gender(Gender v)
    {
        getBlock(Status)->decryptedData()[0x05] = u8(v);
    }

    Language SavSV::language(void) const
    {
        return Language(getBlock(Status)->decryptedData()[0x07]);
    }

    void SavSV::language(Language v)
    {
        getBlock(Status)->decryptedData()[0x07] = u8(v);
    }

    std::string SavSV::otName(void) const
    {
        return StringUtils::getString(getBlock(Status)->decryptedData(), 0x10, 13);
    }

    void SavSV::otName(const std::string_view& v)
    {
        StringUtils::setString(getBlock(Status)->decryptedData(), v, 0x10, 13);
    }

    u32 SavSV::money(void) const
    {
        return LittleEndian::convertTo<u32>(getBlock(KMoney)->decryptedData());
    }

    void SavSV::money(u32 v)
    {
        LittleEndian::convertFrom<u32>(getBlock(KMoney)->decryptedData(), v);
    }

    // SV uses int32 fields for play time (unlike Z-A's double-precision seconds)
    u16 SavSV::playedHours(void) const
    {
        auto block = getBlock(KPlayTime);
        if (!block)
        {
            return 0;
        }
        return (u16)LittleEndian::convertTo<u32>(block->decryptedData());
    }

    void SavSV::playedHours(u16 v)
    {
        auto block = getBlock(KPlayTime);
        if (!block)
        {
            return;
        }
        LittleEndian::convertFrom<u32>(block->decryptedData(), (u32)v);
    }

    u8 SavSV::playedMinutes(void) const
    {
        auto block = getBlock(KPlayTime);
        if (!block)
        {
            return 0;
        }
        return (u8)LittleEndian::convertTo<u32>(block->decryptedData() + 4);
    }

    void SavSV::playedMinutes(u8 v)
    {
        auto block = getBlock(KPlayTime);
        if (!block)
        {
            return;
        }
        LittleEndian::convertFrom<u32>(block->decryptedData() + 4, (u32)v);
    }

    u8 SavSV::playedSeconds(void) const
    {
        auto block = getBlock(KPlayTime);
        if (!block)
        {
            return 0;
        }
        return (u8)LittleEndian::convertTo<u32>(block->decryptedData() + 8);
    }

    void SavSV::playedSeconds(u8 v)
    {
        auto block = getBlock(KPlayTime);
        if (!block)
        {
            return;
        }
        LittleEndian::convertFrom<u32>(block->decryptedData() + 8, (u32)v);
    }

    // Items: stubbed for now
    void SavSV::item(const Item&, Pouch, u16) {}

    std::unique_ptr<Item> SavSV::item(Pouch, u16) const
    {
        return std::make_unique<Item9a>();
    }

    SmallVector<std::pair<Sav::Pouch, int>, 15> SavSV::pouches(void) const
    {
        return {};
    }

    SmallVector<std::pair<Sav::Pouch, std::span<const int>>, 15> SavSV::validItems(void) const
    {
        return {};
    }

    u8 SavSV::currentBox() const
    {
        auto block = getBlock(KCurrentBox);
        if (!block)
        {
            return 0;
        }
        return block->decryptedData()[0];
    }

    void SavSV::currentBox(u8 box)
    {
        auto block = getBlock(KCurrentBox);
        if (!block)
        {
            return;
        }
        block->decryptedData()[0] = box;
    }

    u8 SavSV::unlockedBoxes() const
    {
        auto block = getBlock(KBoxesUnlocked);
        if (!block)
        {
            return maxBoxes();
        }
        return block->decryptedData()[0];
    }

    void SavSV::unlockedBoxes(u8 v)
    {
        auto block = getBlock(KBoxesUnlocked);
        if (!block)
        {
            return;
        }
        block->decryptedData()[0] = v;
    }

    std::string SavSV::boxName(u8 box) const
    {
        return StringUtils::getString(
            getBlock(BoxLayout)->decryptedData(), box * BOX_NAME_BYTES, BOX_NAME_CHARS);
    }

    void SavSV::boxName(u8 box, const std::string_view& name)
    {
        StringUtils::setString(
            getBlock(BoxLayout)->decryptedData(), name, box * BOX_NAME_BYTES, BOX_NAME_CHARS);
    }

    u8 SavSV::boxWallpaper(u8 box) const
    {
        auto block = getBlock(KBoxWallpapers);
        if (!block)
        {
            return 0;
        }
        return block->decryptedData()[box];
    }

    void SavSV::boxWallpaper(u8 box, u8 v)
    {
        auto block = getBlock(KBoxWallpapers);
        if (!block)
        {
            return;
        }
        block->decryptedData()[box] = v;
    }

    // SV: No gap between slots. Each slot is PK9::PARTY_LENGTH (0x158) bytes.
    u32 SavSV::boxOffset(u8 box, u8 slot) const
    {
        return PK9::PARTY_LENGTH * slot + PK9::PARTY_LENGTH * 30 * box;
    }

    u32 SavSV::partyOffset(u8 slot) const
    {
        return PK9::PARTY_LENGTH * slot;
    }

    // SV stores party count as a byte after the 6 party slots
    u8 SavSV::partyCount(void) const
    {
        return getBlock(Party)->decryptedData()[PK9::PARTY_LENGTH * 6];
    }

    void SavSV::partyCount(u8 count)
    {
        getBlock(Party)->decryptedData()[PK9::PARTY_LENGTH * 6] = count;
    }

    std::unique_ptr<PKX> SavSV::pkm(u8 slot) const
    {
        u32 offset = partyOffset(slot);
        return PKX::getPKM<PK9>(getBlock(Party)->decryptedData() + offset, PK9::PARTY_LENGTH);
    }

    std::unique_ptr<PKX> SavSV::pkm(u8 box, u8 slot) const
    {
        u32 offset = boxOffset(box, slot);
        return PKX::getPKM<PK9>(
            getBlock(Box)->decryptedData() + offset, PK9::PARTY_LENGTH);
    }

    void SavSV::pkm(const PKX& pk, u8 box, u8 slot, bool applyTrade)
    {
        if (pk.getLength() == PK9::PARTY_LENGTH || pk.getLength() == PK9::BOX_LENGTH)
        {
            auto pk9 = pk.partyClone();
            if (applyTrade)
            {
                trade(*pk9);
            }
            std::ranges::copy(pk9->rawData().subspan(0, PK9::PARTY_LENGTH),
                getBlock(Box)->decryptedData() + boxOffset(box, slot));
        }
    }

    void SavSV::pkm(const PKX& pk, u8 slot)
    {
        if (pk.getLength() == PK9::PARTY_LENGTH || pk.getLength() == PK9::BOX_LENGTH)
        {
            auto pk9 = pk.partyClone();
            pk9->encrypt();
            std::ranges::copy(pk9->rawData(), getBlock(Party)->decryptedData() + partyOffset(slot));
        }
    }

    void SavSV::cryptBoxData(bool crypted)
    {
        for (u8 box = 0; box < maxBoxes(); box++)
        {
            for (u8 slot = 0; slot < 30; slot++)
            {
                std::unique_ptr<PKX> pk9 = PKX::getPKM<PK9>(
                    getBlock(Box)->decryptedData() + boxOffset(box, slot),
                    PK9::PARTY_LENGTH, true);
                if (!crypted)
                {
                    pk9->encrypt();
                }
            }
        }
    }
}
