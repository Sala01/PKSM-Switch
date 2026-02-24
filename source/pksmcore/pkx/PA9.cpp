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

#include "pkx/PA9.hpp"
#include "utils/crypto.hpp"
#include "utils/endian.hpp"
#include "utils/flagUtil.hpp"
#include "utils/utils.hpp"

namespace pksm
{
    void PA9::encrypt(void)
    {
        if (!isEncrypted())
        {
            u8 sv = (encryptionConstant() >> 13) & 31;
            refreshChecksum();
            pksm::crypto::pkm::blockShuffle<BLOCK_LENGTH>(
                data + ENCRYPTION_START, pksm::crypto::pkm::InvertedBlockPositions[sv]);
            pksm::crypto::pkm::crypt<BOX_LENGTH - ENCRYPTION_START>(
                data + ENCRYPTION_START, encryptionConstant());
            if (isParty())
            {
                pksm::crypto::pkm::crypt<PARTY_LENGTH - BOX_LENGTH>(
                    data + BOX_LENGTH, encryptionConstant());
            }
        }
    }

    void PA9::decrypt(void)
    {
        if (isEncrypted())
        {
            u8 sv = (encryptionConstant() >> 13) & 31;
            pksm::crypto::pkm::crypt<BOX_LENGTH - ENCRYPTION_START>(
                data + ENCRYPTION_START, encryptionConstant());
            if (isParty())
            {
                pksm::crypto::pkm::crypt<PARTY_LENGTH - BOX_LENGTH>(
                    data + BOX_LENGTH, encryptionConstant());
            }
            pksm::crypto::pkm::blockShuffle<BLOCK_LENGTH>(data + ENCRYPTION_START, sv);
        }
    }

    bool PA9::isEncrypted() const
    {
        return LittleEndian::convertTo<u16>(data + 0x70) != 0 ||
               LittleEndian::convertTo<u16>(data + 0x110) != 0;
    }

    PA9::PA9(PrivateConstructor, u8* dt, bool party, bool direct)
        : PKX(dt, party ? PARTY_LENGTH : BOX_LENGTH, direct)
    {
        if (isEncrypted())
        {
            decrypt();
        }
    }

    std::unique_ptr<PKX> PA9::clone(void) const
    {
        return PKX::getPKM<PA9>(data, isParty() ? PARTY_LENGTH : BOX_LENGTH);
    }

    Generation PA9::generation(void) const
    {
        return Generation::NINE;
    }

    u32 PA9::encryptionConstant(void) const
    {
        return LittleEndian::convertTo<u32>(data);
    }

    void PA9::encryptionConstant(u32 v)
    {
        LittleEndian::convertFrom<u32>(data, v);
    }

    u16 PA9::sanity(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x04);
    }

    void PA9::sanity(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x04, v);
    }

    u16 PA9::checksum(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x06);
    }

    void PA9::checksum(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x06, v);
    }

    Species PA9::species(void) const
    {
        return Species{LittleEndian::convertTo<u16>(data + 0x08)};
    }

    void PA9::species(Species v)
    {
        LittleEndian::convertFrom<u16>(data + 0x08, u16(v));
    }

    u16 PA9::heldItem(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x0A);
    }

    void PA9::heldItem(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x0A, v);
    }

    u16 PA9::TID(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x0C);
    }

    void PA9::TID(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x0C, v);
    }

    u16 PA9::SID(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x0E);
    }

    void PA9::SID(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x0E, v);
    }

    u32 PA9::experience(void) const
    {
        return LittleEndian::convertTo<u32>(data + 0x10);
    }

    void PA9::experience(u32 v)
    {
        LittleEndian::convertFrom<u32>(data + 0x10, v);
    }

    Ability PA9::ability(void) const
    {
        return Ability{LittleEndian::convertTo<u16>(data + 0x14)};
    }

    void PA9::ability(Ability v)
    {
        LittleEndian::convertFrom<u16>(data + 0x14, u16(v));
    }

    void PA9::setAbility(u8 v)
    {
        u8 abilitynum;

        if (v == 0)
        {
            abilitynum = 1;
        }
        else if (v == 1)
        {
            abilitynum = 2;
        }
        else
        {
            abilitynum = 4;
        }

        abilityNumber(abilitynum);
        ability(abilities(v));
    }

    u8 PA9::abilityNumber(void) const
    {
        return data[0x16] & 0x7;
    }

    void PA9::abilityNumber(u8 v)
    {
        data[0x16] = (data[0x16] & ~7) | (v & 7);
    }

    u16 PA9::markValue(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x18);
    }

    void PA9::markValue(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x18, v);
    }

    u32 PA9::PID(void) const
    {
        return LittleEndian::convertTo<u32>(data + 0x1C);
    }

    void PA9::PID(u32 v)
    {
        LittleEndian::convertFrom<u32>(data + 0x1C, v);
    }

    Nature PA9::nature(void) const
    {
        return Nature{data[0x20]};
    }

    void PA9::nature(Nature v)
    {
        data[0x20] = u8(v);
        data[0x21] = u8(v);
    }

    bool PA9::fatefulEncounter(void) const
    {
        return (data[0x22] & 1) == 1;
    }

    void PA9::fatefulEncounter(bool v)
    {
        data[0x22] = (u8)((data[0x22] & ~0x01) | (v ? 1 : 0));
    }

    Gender PA9::gender(void) const
    {
        return Gender{u8((data[0x22] >> 2) & 0x3)};
    }

    void PA9::gender(Gender v)
    {
        data[0x22] = (data[0x22] & ~12) | ((u8(v) & 3) << 2);
    }

    u16 PA9::alternativeForm(void) const
    {
        return data[0x24];
    }

    void PA9::alternativeForm(u16 v)
    {
        data[0x24] = v;
    }

    u16 PA9::ev(Stat ev) const
    {
        return data[0x26 + u8(ev)];
    }

    void PA9::ev(Stat ev, u16 v)
    {
        data[0x26 + u8(ev)] = v;
    }

    u8 PA9::contest(u8 contest) const
    {
        return data[0x2C + contest];
    }

    void PA9::contest(u8 contest, u8 v)
    {
        data[0x2C + contest] = v;
    }

    u8 PA9::pkrs(void) const
    {
        return data[0x32];
    }

    void PA9::pkrs(u8 v)
    {
        data[0x32] = v;
    }

    u8 PA9::pkrsDays(void) const
    {
        return data[0x32] & 0xF;
    }

    void PA9::pkrsDays(u8 v)
    {
        data[0x32] = (data[0x32] & ~0xF) | (v & 0xF);
    }

    u8 PA9::pkrsStrain(void) const
    {
        return data[0x32] >> 4;
    }

    void PA9::pkrsStrain(u8 v)
    {
        data[0x32] = (data[0x32] & 0xF) | (v << 4);
    }

    bool PA9::hasRibbon(Ribbon) const
    {
        return false;
    }

    bool PA9::ribbon(Ribbon) const
    {
        return false;
    }

    void PA9::ribbon(Ribbon, bool)
    {
    }

    std::string PA9::nickname(void) const
    {
        return StringUtils::transString67(StringUtils::getString(data, 0x58, 13));
    }

    void PA9::nickname(const std::string_view& v)
    {
        StringUtils::setString(data, StringUtils::transString67(v), 0x58, 13);
    }

    Move PA9::move(u8 m) const
    {
        return Move{LittleEndian::convertTo<u16>(data + 0x72 + m * 2)};
    }

    void PA9::move(u8 m, Move v)
    {
        LittleEndian::convertFrom<u16>(data + 0x72 + m * 2, u16(v));
    }

    u8 PA9::PP(u8 m) const
    {
        return data[0x7A + m];
    }

    void PA9::PP(u8 m, u8 v)
    {
        data[0x7A + m] = v;
    }

    u8 PA9::PPUp(u8 m) const
    {
        return data[0x7E + m];
    }

    void PA9::PPUp(u8 m, u8 v)
    {
        data[0x7E + m] = v;
    }

    Move PA9::relearnMove(u8 m) const
    {
        return Move{LittleEndian::convertTo<u16>(data + 0x82 + m * 2)};
    }

    void PA9::relearnMove(u8 m, Move v)
    {
        LittleEndian::convertFrom<u16>(data + 0x82 + m * 2, u16(v));
    }

    int PA9::partyCurrHP(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x8A);
    }

    void PA9::partyCurrHP(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x8A, v);
    }

    u8 PA9::iv(Stat stat) const
    {
        u32 buffer = LittleEndian::convertTo<u32>(data + 0x8C);
        return (u8)((buffer >> 5 * u8(stat)) & 0x1F);
    }

    void PA9::iv(Stat stat, u8 v)
    {
        u32 buffer = LittleEndian::convertTo<u32>(data + 0x8C);
        buffer &= ~(0x1F << 5 * u8(stat));
        buffer |= v << (5 * u8(stat));
        LittleEndian::convertFrom<u32>(data + 0x8C, buffer);
    }

    bool PA9::egg(void) const
    {
        return ((LittleEndian::convertTo<u32>(data + 0x8C) >> 30) & 0x1) == 1;
    }

    void PA9::egg(bool v)
    {
        LittleEndian::convertFrom<u32>(
            data + 0x8C, (u32)((LittleEndian::convertTo<u32>(data + 0x8C) & ~0x40000000) |
                               (u32)(v ? 0x40000000 : 0)));
    }

    bool PA9::nicknamed(void) const
    {
        return ((LittleEndian::convertTo<u32>(data + 0x8C) >> 31) & 0x1) == 1;
    }

    void PA9::nicknamed(bool v)
    {
        LittleEndian::convertFrom<u32>(data + 0x8C,
            (LittleEndian::convertTo<u32>(data + 0x8C) & 0x7FFFFFFF) | (v ? 0x80000000 : 0));
    }

    bool PA9::hyperTrain(Stat stat) const
    {
        return (data[0x126] & (1 << u8(stat))) != 0;
    }

    void PA9::hyperTrain(Stat stat, bool v)
    {
        data[0x126] = (data[0x126] & ~(1 << u8(stat))) | (v ? (1 << u8(stat)) : 0);
    }

    PKXHandler PA9::currentHandler(void) const
    {
        return data[0xC4] == 0 ? PKXHandler::OT : PKXHandler::NonOT;
    }

    void PA9::currentHandler(PKXHandler v)
    {
        data[0xC4] = v == PKXHandler::OT ? 0 : 1;
    }

    std::string PA9::otName(void) const
    {
        return StringUtils::transString67(StringUtils::getString(data, 0xF8, 13));
    }

    void PA9::otName(const std::string_view& v)
    {
        StringUtils::setString(data, StringUtils::transString67(v), 0xF8, 13);
    }

    u8 PA9::htFriendship(void) const
    {
        return data[0xC8];
    }

    void PA9::htFriendship(u8 v)
    {
        data[0xC8] = v;
    }

    u8 PA9::otFriendship(void) const
    {
        return data[0x112];
    }

    void PA9::otFriendship(u8 v)
    {
        data[0x112] = v;
    }

    u16 PA9::eggLocation(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x120);
    }

    void PA9::eggLocation(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x120, v);
    }

    u16 PA9::metLocation(void) const
    {
        return LittleEndian::convertTo<u16>(data + 0x122);
    }

    void PA9::metLocation(u16 v)
    {
        LittleEndian::convertFrom<u16>(data + 0x122, v);
    }

    Ball PA9::ball(void) const
    {
        return Ball{data[0x124]};
    }

    void PA9::ball(Ball v)
    {
        data[0x124] = u8(v);
    }

    u8 PA9::metLevel(void) const
    {
        return data[0x125] & ~0x80;
    }

    void PA9::metLevel(u8 v)
    {
        data[0x125] = (data[0x125] & 0x80) | v;
    }

    Gender PA9::otGender(void) const
    {
        return Gender{u8(data[0x125] >> 7)};
    }

    void PA9::otGender(Gender v)
    {
        data[0x125] = (data[0x125] & ~0x80) | (u8(v) << 7);
    }

    GameVersion PA9::version(void) const
    {
        return GameVersion(data[0xCE]);
    }

    void PA9::version(GameVersion v)
    {
        data[0xCE] = u8(v);
    }

    Language PA9::language(void) const
    {
        return Language(data[0xD5]);
    }

    void PA9::language(Language v)
    {
        data[0xD5] = u8(v);
    }

    int PA9::partyStat(Stat stat) const
    {
        if (!isParty())
        {
            return -1;
        }
        return LittleEndian::convertTo<u16>(data + 0x14A + u8(stat) * 2);
    }

    void PA9::partyStat(Stat stat, u16 v)
    {
        if (isParty())
        {
            LittleEndian::convertFrom<u16>(data + 0x14A + u8(stat) * 2, v);
        }
    }

    int PA9::partyLevel() const
    {
        if (!isParty())
        {
            return -1;
        }
        return *(data + 0x148);
    }

    void PA9::partyLevel(u8 v)
    {
        if (isParty())
        {
            *(data + 0x148) = v;
        }
    }

    void PA9::updatePartyData(void)
    {
        if (!isParty())
        {
            return;
        }

        partyLevel(level());
        partyCurrHP(stat(Stat::HP));
        for (Stat s : {Stat::HP, Stat::ATK, Stat::DEF, Stat::SPD, Stat::SPATK, Stat::SPDEF})
        {
            partyStat(s, stat(s));
        }
    }

    void PA9::refreshChecksum(void)
    {
        u16 chk = 0;
        for (size_t i = 8; i < BOX_LENGTH; i += 2)
        {
            chk += LittleEndian::convertTo<u16>(data + i);
        }
        checksum(chk);
    }

    Type PA9::hpType(void) const
    {
        return Type{u8((15 *
                           ((iv(Stat::HP) & 1) + 2 * (iv(Stat::ATK) & 1) + 4 * (iv(Stat::DEF) & 1) +
                               8 * (iv(Stat::SPD) & 1) + 16 * (iv(Stat::SPATK) & 1) +
                               32 * (iv(Stat::SPDEF) & 1)) /
                           63) +
                       1)};
    }

    void PA9::hpType(Type)
    {
    }

    u16 PA9::TSV(void) const
    {
        return (TID() ^ SID()) >> 4;
    }

    u16 PA9::PSV(void) const
    {
        return ((PID() >> 16) ^ (PID() & 0xFFFF)) >> 4;
    }

    u8 PA9::level(void) const
    {
        if (isParty())
        {
            return data[0x148];
        }

        u8 i      = 1;
        u8 xpType = expType();
        while (experience() >= expTable(i, xpType) && ++i < 100)
        {
            ;
        }
        return i;
    }

    void PA9::level(u8 v)
    {
        experience(expTable(v - 1, expType()));
        if (isParty())
        {
            data[0x148] = v;
        }
    }

    bool PA9::shiny(void) const
    {
        return PSV() == TSV();
    }

    void PA9::shiny(bool)
    {
    }

    u16 PA9::formSpecies(void) const
    {
        return u16(species());
    }

    u16 PA9::statImpl(Stat stat) const
    {
        // No full stat calculation support; for party data this is sufficient for now.
        switch (stat)
        {
            case Stat::HP:
                return isParty() ? LittleEndian::convertTo<u16>(data + 0x14A) : 0;
            case Stat::ATK:
                return isParty() ? LittleEndian::convertTo<u16>(data + 0x14C) : 0;
            case Stat::DEF:
                return isParty() ? LittleEndian::convertTo<u16>(data + 0x14E) : 0;
            case Stat::SPD:
                return isParty() ? LittleEndian::convertTo<u16>(data + 0x150) : 0;
            case Stat::SPATK:
                return isParty() ? LittleEndian::convertTo<u16>(data + 0x152) : 0;
            case Stat::SPDEF:
                return isParty() ? LittleEndian::convertTo<u16>(data + 0x154) : 0;
        }
        return 0;
    }

    int PA9::eggYear(void) const
    {
        return 2000 + data[0x119];
    }

    void PA9::eggYear(int v)
    {
        data[0x119] = v - 2000;
    }

    int PA9::eggMonth(void) const
    {
        return data[0x11A];
    }

    void PA9::eggMonth(int v)
    {
        data[0x11A] = v;
    }

    int PA9::eggDay(void) const
    {
        return data[0x11B];
    }

    void PA9::eggDay(int v)
    {
        data[0x11B] = v;
    }

    int PA9::metYear(void) const
    {
        return 2000 + data[0x11C];
    }

    void PA9::metYear(int v)
    {
        data[0x11C] = v - 2000;
    }

    int PA9::metMonth(void) const
    {
        return data[0x11D];
    }

    void PA9::metMonth(int v)
    {
        data[0x11D] = v;
    }

    int PA9::metDay(void) const
    {
        return data[0x11E];
    }

    void PA9::metDay(int v)
    {
        data[0x11E] = v;
    }
}
