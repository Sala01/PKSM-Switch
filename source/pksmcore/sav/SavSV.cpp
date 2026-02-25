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
    // Item9a size
    constexpr int ITEM_SIZE = 16;

    // PokeDexEntry9Paldea: 0x18 bytes per species
    constexpr int DEX_ENTRY_SIZE = 0x18;

    // Gen 9 species converter: national dex ID -> internal ID
    // Species 0-916 are identity. From 917, apply delta from table.
    constexpr int FIRST_UNALIGNED_9 = 917;
    constexpr int8_t speciesTable9[] = {
                                           1,  1,  1,
         1, 33, 33, 33, 21, 21, 44, 44,  7,  7,
         7, 29, 31, 31, 31, 68, 68, 68,  2,  2,
        17, 17, 30, 30, 24, 24, 28, 28, 58, 58,
        12,-13,-13,-31,-31,-29,-29, 43, 43, 43,
       -31,-31, -3,-30,-30,-23,-23,-14,-24, -3,
        -3,-47,-47,-12,-27,-27,-44,-46,-26, 31,
        29,-53,-65, 25, -6, -3, -7, -4, -4, -8,
        -4,  1, -3, -3, -6, -4,-47,-47,-47,-23,
       -23, -5, -7, -9, -7,-20,-13, -9, -9,-29,
       -23,  1, 12, 12,  0,  0,  0, -6,  5, -6,
        -3, -3, -2, -4, -3, -3,
    };

    u16 getInternal9(u16 species)
    {
        int shift = species - FIRST_UNALIGNED_9;
        if (shift < 0 || shift >= (int)std::size(speciesTable9))
        {
            return species;
        }
        return u16(species + speciesTable9[shift]);
    }

    // Dex language flag: convert language ID to bit index
    // Languages 1-5 -> bits 0-4, skip 6, languages 7-10 -> bits 5-8
    int getDexLangFlag(int lang)
    {
        if (lang > 10 || lang == 6 || lang <= 0)
        {
            return 0;
        }
        return lang >= 7 ? lang - 2 : lang - 1;
    }

    // Item lists from ItemStorage9SV.cs
    constexpr int medicineItems[] = {
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
        27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 708, 709,
    };

    constexpr int ballItems[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        11, 12, 13, 14, 15, 16, 492, 493, 494, 495,
        496, 497, 498, 499, 500, 576, 851, 1785,
    };

    constexpr int battleItems[] = {
        55, 56, 57, 58, 59, 60, 61, 62, 63,
    };

    constexpr int berryItems[] = {
        149, 150, 151, 152, 153, 154, 155, 156, 157, 158,
        159, 160, 161, 162, 163, 169, 170, 171, 172, 173,
        174, 184, 185, 186, 187, 188, 189, 190, 191, 192,
        193, 194, 195, 196, 197, 198, 199, 200, 201, 202,
        203, 204, 205, 206, 207, 208, 209, 210, 211, 212,
        686, 687, 688,
    };

    constexpr int otherItems[] = {
        45, 46, 47, 48, 49, 50, 51, 52, 53, 80,
        81, 82, 83, 84, 85, 107, 108, 109, 110, 111,
        112, 135, 136, 213, 214, 217, 218, 219, 220, 221,
        222, 223, 224, 225, 228, 229, 230, 231, 232, 233,
        234, 235, 236, 237, 238, 239, 240, 241, 242, 243,
        244, 245, 246, 247, 248, 249, 250, 251, 252, 253,
        265, 266, 267, 268, 269, 270, 271, 272, 273, 275,
        276, 277, 278, 279, 280, 281, 282, 283, 284, 285,
        286, 287, 288, 289, 290, 291, 292, 293, 294, 295,
        296, 297, 298, 299, 300, 301, 302, 303, 304, 305,
        306, 307, 308, 309, 310, 311, 312, 313, 321, 322,
        323, 324, 325, 326, 327, 485, 486, 487, 488, 489,
        490, 491, 537, 538, 539, 540, 541, 542, 543, 544,
        545, 546, 547, 564, 565, 566, 567, 568, 569, 570,
        639, 640, 644, 645, 648, 649, 650, 795, 796, 846,
        849, 853, 854, 855, 856, 879, 880, 881, 882, 883,
        884, 1103, 1104, 1109, 1110, 1111, 1112, 1113, 1114, 1115,
        1116, 1117, 1118, 1119, 1120, 1121, 1122, 1123, 1124, 1125,
        1126, 1127, 1128, 1231, 1232, 1233, 1234, 1235, 1236, 1237,
        1238, 1239, 1240, 1241, 1242, 1243, 1244, 1245, 1246, 1247,
        1248, 1249, 1250, 1251, 1253, 1254, 1581, 1582, 1592, 1606,
        1777, 1778, 1779, 1861, 1862, 1863, 1864, 1865, 1866, 1867,
        1868, 1869, 1870, 1871, 1872, 1873, 1874, 1875, 1876, 1877,
        1878, 1879, 1880, 1881, 1882, 1883, 1884, 1885, 1886, 2344,
        2345, 2401, 2402, 2403, 2404, 2406, 2407, 2408, 2411, 2412,
        2413, 2414, 2415, 2416, 2479, 2482, 2549,
    };

    constexpr int tmItems[] = {
        328, 329, 330, 331, 332, 333, 334, 335, 336, 337,
        338, 339, 340, 341, 342, 343, 344, 345, 346, 347,
        348, 349, 350, 351, 352, 353, 354, 355, 356, 357,
        358, 359, 360, 361, 362, 363, 364, 365, 366, 367,
        368, 369, 370, 371, 372, 373, 374, 375, 376, 377,
        378, 379, 380, 381, 382, 383, 384, 385, 386, 387,
        388, 389, 390, 391, 392, 393, 394, 395, 396, 397,
        398, 399, 400, 401, 402, 403, 404, 405, 406, 407,
        408, 409, 410, 411, 412, 413, 414, 415, 416, 417,
        418, 419, 618, 619, 620, 690, 691, 692, 693, 1230,
        2160, 2161, 2162, 2163, 2164, 2165, 2166, 2167, 2168, 2169,
        2170, 2171, 2172, 2173, 2174, 2175, 2176, 2177, 2178, 2179,
        2180, 2181, 2182, 2183, 2184, 2185, 2186, 2187, 2188, 2189,
        2190, 2191, 2192, 2193, 2194, 2195, 2196, 2197, 2198, 2199,
        2200, 2201, 2202, 2203, 2204, 2205, 2206, 2207, 2208, 2209,
        2210, 2211, 2212, 2213, 2214, 2215, 2216, 2217, 2218, 2219,
        2220, 2221, 2222, 2223, 2224, 2225, 2226, 2227, 2228, 2229,
        2230, 2231, 2232, 2233, 2234, 2235, 2236, 2237, 2238, 2239,
        2240, 2241, 2242, 2243, 2244, 2245, 2246, 2247, 2248, 2249,
        2250, 2251, 2252, 2253, 2254, 2255, 2256, 2257, 2258, 2259,
        2260, 2261, 2262, 2263, 2264, 2265, 2266, 2267, 2268, 2269,
        2270, 2271, 2272, 2273, 2274, 2275, 2276, 2277, 2278, 2279,
        2280, 2281, 2282, 2283, 2284, 2285, 2286, 2287, 2288, 2289,
    };

    constexpr int treasureItems[] = {
        86, 87, 88, 89, 90, 91, 92, 94, 106, 571,
        580, 581, 582, 583, 1842, 1843,
    };

    constexpr int picnicItems[] = {
        1888, 1889, 1890, 1891, 1892, 1893, 1894, 1895, 1896, 1897,
        1898, 1899, 1900, 1901, 1902, 1903, 1904, 1905, 1906, 1907,
        1908, 1909, 1910, 1911, 1912, 1913, 1914, 1915, 1916, 1917,
        1918, 1919, 1920, 1921, 1922, 1923, 1924, 1925, 1926, 1927,
        1928, 1929, 1930, 1931, 1932, 1933, 1934, 1935, 1936, 1937,
        1938, 1939, 1940, 1941, 1942, 1943, 1944, 1945, 1946, 2311,
        2313, 2314, 2315, 2316, 2317, 2318, 2319, 2320, 2321, 2322,
        2323, 2324, 2325, 2326, 2327, 2329, 2330, 2331, 2332, 2333,
        2334, 2335, 2336, 2337, 2338, 2339, 2340, 2341, 2342, 2348,
        2349, 2350, 2351, 2352, 2353, 2354, 2355, 2356, 2357, 2358,
        2359, 2360, 2361, 2362, 2363, 2364, 2365, 2366, 2367, 2368,
        2369, 2370, 2371, 2372, 2373, 2374, 2375, 2376, 2377, 2378,
        2379, 2380, 2381, 2382, 2383, 2384, 2385, 2386, 2387, 2388,
        2389, 2390, 2391, 2392, 2393, 2394, 2395, 2396, 2397, 2398,
        2399, 2400, 2417, 2418, 2419, 2420, 2421, 2422, 2423, 2424,
        2425, 2426, 2427, 2428, 2429, 2430, 2431, 2432, 2433, 2434,
        2435, 2436, 2437, 2548, 2551, 2552,
    };

    constexpr int keyItems[] = {
        78, 466, 628, 629, 631, 632, 638, 703, 765, 943,
        944, 945, 946, 1267, 1278, 1587, 1589, 1590, 1591, 1829,
        1830, 1831, 1832, 1833, 1834, 1835, 1836, 1857, 1858, 2405,
        2409, 2410, 2480, 2481, 2483, 2522, 2523, 2524, 2525, 2526,
        2527, 2528, 2529, 2530, 2531, 2532, 2533, 2534, 2535, 2536,
        2537, 2538, 2539, 2540, 2541, 2542, 2543, 2544, 2545, 2546,
        2547, 2550, 2553, 2554, 2555, 2556, 2557,
    };

    constexpr int materialItems[] = {
        1956, 1957, 1958, 1959, 1960, 1961, 1962, 1963, 1964, 1965,
        1966, 1967, 1968, 1969, 1970, 1971, 1972, 1973, 1974, 1975,
        1976, 1977, 1978, 1979, 1980, 1981, 1982, 1983, 1984, 1985,
        1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
        1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
        2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015,
        2016, 2017, 2018, 2019, 2020, 2021, 2022, 2023, 2024, 2025,
        2026, 2027, 2028, 2029, 2030, 2031, 2032, 2033, 2034, 2035,
        2036, 2037, 2038, 2039, 2040, 2041, 2042, 2043, 2044, 2045,
        2046, 2047, 2048, 2049, 2050, 2051, 2052, 2053, 2054, 2055,
        2056, 2057, 2058, 2059, 2060, 2061, 2062, 2063, 2064, 2065,
        2066, 2067, 2068, 2069, 2070, 2071, 2072, 2073, 2074, 2075,
        2076, 2077, 2078, 2079, 2080, 2081, 2082, 2083, 2084, 2085,
        2086, 2087, 2088, 2089, 2090, 2091, 2092, 2093, 2094, 2095,
        2096, 2097, 2098, 2099, 2103, 2104, 2105, 2106, 2107, 2108,
        2109, 2110, 2111, 2112, 2113, 2114, 2115, 2116, 2117, 2118,
        2119, 2120, 2121, 2122, 2123, 2126, 2127, 2128, 2129, 2130,
        2131, 2132, 2133, 2134, 2135, 2136, 2137, 2156, 2157, 2158,
        2159, 2438, 2439, 2440, 2441, 2442, 2443, 2444, 2445, 2446,
        2447, 2448, 2449, 2450, 2451, 2452, 2453, 2454, 2455, 2456,
        2457, 2458, 2459, 2460, 2461, 2462, 2463, 2464, 2465, 2466,
        2467, 2468, 2469, 2470, 2471, 2472, 2473, 2474, 2475, 2476,
        2477, 2478, 2484, 2485, 2486, 2487, 2488, 2489, 2490, 2491,
        2492, 2493, 2494, 2495, 2496, 2497, 2498, 2499, 2500, 2501,
        2502, 2503, 2504, 2505, 2506, 2507, 2508, 2509, 2510, 2511,
        2512, 2513, 2514, 2515, 2516, 2517, 2518, 2519, 2520, 2521,
    };

    std::span<const int> itemListForPouch(pksm::Sav::Pouch pouch)
    {
        using P = pksm::Sav::Pouch;
        switch (pouch)
        {
            case P::Medicine:
                return medicineItems;
            case P::Ball:
                return ballItems;
            case P::Battle:
                return battleItems;
            case P::Berry:
                return berryItems;
            case P::NormalItem:
                return otherItems;
            case P::TM:
                return tmItems;
            case P::Treasure:
                return treasureItems;
            case P::Ingredient:
                return picnicItems;
            case P::KeyItem:
                return keyItems;
            case P::Candy:
                return materialItems;
            default:
                return {};
        }
    }
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

    u32 SavSV::BP(void) const
    {
        auto block = getBlock(KLeaguePoints);
        if (!block)
        {
            return 0;
        }
        return LittleEndian::convertTo<u32>(block->decryptedData());
    }

    void SavSV::BP(u32 v)
    {
        auto block = getBlock(KLeaguePoints);
        if (!block)
        {
            return;
        }
        LittleEndian::convertFrom<u32>(block->decryptedData(), v);
    }

    u8 SavSV::badges(void) const
    {
        static constexpr u32 gymBadgeKeys[] = {
            KBadgeElectric, KBadgePsychic, KBadgeGhost, KBadgeIce,
            KBadgeGrass, KBadgeWater, KBadgeBug, KBadgeNormal,
        };
        u8 count = 0;
        for (auto key : gymBadgeKeys)
        {
            auto block = getBlock(key);
            if (block && block->decryptedData()[0] != 0)
            {
                count++;
            }
        }
        return count;
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

    // Items: SV uses index-based storage (3000 slots x 16 bytes, indexed by item ID)
    // Format per slot: [Pouch:u32][Count:u32][Flags:u32][Padding:u32]
    void SavSV::item(const Item& item, Pouch pouch, u16 slot)
    {
        auto items = itemListForPouch(pouch);
        if (slot >= items.size())
        {
            return;
        }
        u16 itemId    = static_cast<u16>(items[slot]);
        u8* blockData = getBlock(Items)->decryptedData();
        u32 offset    = itemId * ITEM_SIZE;
        u32 count     = item.count();
        LittleEndian::convertFrom<u32>(blockData + offset + 4, count);
    }

    std::unique_ptr<Item> SavSV::item(Pouch pouch, u16 slot) const
    {
        auto items = itemListForPouch(pouch);
        if (slot >= items.size())
        {
            return std::make_unique<Item9a>();
        }
        u16 itemId    = static_cast<u16>(items[slot]);
        u8* blockData = getBlock(Items)->decryptedData();
        u32 offset    = itemId * ITEM_SIZE;
        return std::make_unique<Item9a>(blockData + offset, itemId);
    }

    SmallVector<std::pair<Sav::Pouch, int>, 15> SavSV::pouches(void) const
    {
        return {
            std::pair{Pouch::Medicine,   int(std::size(medicineItems))},
            std::pair{Pouch::Ball,       int(std::size(ballItems))},
            std::pair{Pouch::Battle,     int(std::size(battleItems))},
            std::pair{Pouch::Berry,      int(std::size(berryItems))},
            std::pair{Pouch::NormalItem, int(std::size(otherItems))},
            std::pair{Pouch::TM,         int(std::size(tmItems))},
            std::pair{Pouch::Treasure,   int(std::size(treasureItems))},
            std::pair{Pouch::Ingredient, int(std::size(picnicItems))},
            std::pair{Pouch::KeyItem,    int(std::size(keyItems))},
            std::pair{Pouch::Candy,      int(std::size(materialItems))},
        };
    }

    SmallVector<std::pair<Sav::Pouch, std::span<const int>>, 15> SavSV::validItems(void) const
    {
        return {
            std::pair{Pouch::Medicine,   std::span<const int>(medicineItems)},
            std::pair{Pouch::Ball,       std::span<const int>(ballItems)},
            std::pair{Pouch::Battle,     std::span<const int>(battleItems)},
            std::pair{Pouch::Berry,      std::span<const int>(berryItems)},
            std::pair{Pouch::NormalItem, std::span<const int>(otherItems)},
            std::pair{Pouch::TM,        std::span<const int>(tmItems)},
            std::pair{Pouch::Treasure,   std::span<const int>(treasureItems)},
            std::pair{Pouch::Ingredient, std::span<const int>(picnicItems)},
            std::pair{Pouch::KeyItem,    std::span<const int>(keyItems)},
            std::pair{Pouch::Candy,      std::span<const int>(materialItems)},
        };
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

    // Pokedex: SV uses PokeDexEntry9Paldea (0x18 bytes per species), indexed by internal species ID
    void SavSV::dex(const PKX& pk)
    {
        if (pk.egg())
        {
            return;
        }
        auto block = getBlock(KZukan);
        if (!block)
        {
            return;
        }

        u16 species  = u16(pk.species());
        u16 internal = getInternal9(species);
        u8* entry    = block->decryptedData() + internal * DEX_ENTRY_SIZE;

        // State: set to 3 (caught)
        LittleEndian::convertFrom<u32>(entry + 0x00, 3u);

        // Form seen flag
        u8 form       = pk.alternativeForm();
        u32 formFlags = LittleEndian::convertTo<u32>(entry + 0x04);
        formFlags |= (1u << form);
        LittleEndian::convertFrom<u32>(entry + 0x04, formFlags);

        // Gender seen
        u16 genderFlags = LittleEndian::convertTo<u16>(entry + 0x08);
        genderFlags |= u16(1u << u8(pk.gender()));
        LittleEndian::convertFrom<u16>(entry + 0x08, genderFlags);

        // Language obtained (pokemon's language + player's language)
        u16 langFlags = LittleEndian::convertTo<u16>(entry + 0x0A);
        langFlags |= u16(1u << getDexLangFlag(u8(pk.language())));
        langFlags |= u16(1u << getDexLangFlag(u8(language())));
        LittleEndian::convertFrom<u16>(entry + 0x0A, langFlags);

        // Shiny
        if (pk.shiny())
        {
            entry[0x0C] = 1;
        }

        // Display form, gender, shiny
        LittleEndian::convertFrom<u32>(entry + 0x10, u32(form));
        entry[0x14] = u8(pk.gender());
        if (pk.shiny())
        {
            entry[0x15] = 1;
        }
    }

    int SavSV::dexSeen(void) const
    {
        auto block = getBlock(KZukan);
        if (!block)
        {
            return 0;
        }
        u8* data = block->decryptedData();
        int count = 0;
        for (const auto& spec : availableSpecies())
        {
            u16 internal = getInternal9(u16(spec));
            u32 state    = LittleEndian::convertTo<u32>(data + internal * DEX_ENTRY_SIZE);
            if (state >= 2)
            {
                count++;
            }
        }
        return count;
    }

    int SavSV::dexCaught(void) const
    {
        auto block = getBlock(KZukan);
        if (!block)
        {
            return 0;
        }
        u8* data = block->decryptedData();
        int count = 0;
        for (const auto& spec : availableSpecies())
        {
            u16 internal = getInternal9(u16(spec));
            u32 state    = LittleEndian::convertTo<u32>(data + internal * DEX_ENTRY_SIZE);
            if (state >= 3)
            {
                count++;
            }
        }
        return count;
    }
}
