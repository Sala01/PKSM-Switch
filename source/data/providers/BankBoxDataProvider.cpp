#include "data/providers/BankBoxDataProvider.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "pksmcore/enums/Generation.hpp"
#include "pksmcore/pkx/PB7.hpp"
#include "pksmcore/pkx/PK1.hpp"
#include "pksmcore/pkx/PK2.hpp"
#include "pksmcore/pkx/PK3.hpp"
#include "pksmcore/pkx/PK4.hpp"
#include "pksmcore/pkx/PK5.hpp"
#include "pksmcore/pkx/PK6.hpp"
#include "pksmcore/pkx/PK7.hpp"
#include "pksmcore/pkx/PA8.hpp"
#include "pksmcore/pkx/PK8.hpp"
#include "pksmcore/pkx/PK9.hpp"
#include "pksmcore/pkx/PKX.hpp"
#include "utils/Logger.hpp"

namespace {

static constexpr const char *BANKS_ROOT = "sdmc:/switch/PKSM/banks";
static constexpr const char *BANK_MAGIC = "PKSMBANK";
static constexpr uint32_t BANK_VERSION = 4;       // v4: expanded data buffer for PA8 support
static constexpr uint32_t BANK_VERSION_V3 = 3;    // v3: legacy format with 0x148 data buffer
static constexpr int SLOTS_PER_BOX = 30;
static constexpr size_t BANK_DATA_SIZE = 0x168;    // Fits all PKX formats including PA8 (0x168)

struct BankHeader {
    char magic[8];
    uint32_t version;
    uint32_t boxes;
};
static_assert(sizeof(BankHeader) == 16);

// v4 entry: 0x170 bytes (gen + 0x168 data + 4 padding)
// padding[0] is used as a format tag to distinguish PKX sub-types within a generation:
//   0xFF = default (PK8 for Gen 8, PK9 for Gen 9)
//   0x01 = PA8 (Legends: Arceus)
//   0x02 = PA9 (Legends: Z-A)
struct BankEntry {
    pksm::Generation gen;
    uint8_t data[BANK_DATA_SIZE];
    uint8_t padding[4];
};
static_assert(sizeof(BankEntry) == 0x170);

// v3 legacy entry: 0x150 bytes (gen + 0x148 data + 4 padding)
// Used only during v3→v4 migration
struct BankEntryV3 {
    pksm::Generation gen;
    uint8_t data[0x148];
    uint8_t padding[4];
};
static_assert(sizeof(BankEntryV3) == 0x150);

std::filesystem::path BankFilePath(const std::string &root, const std::string &bankName) {
    return std::filesystem::path(root) / (bankName + ".bnk");
}

std::filesystem::path NamesFilePath(const std::string &root, const std::string &bankName) {
    return std::filesystem::path(root) / (bankName + ".json");
}

void EnsureDirExists(const std::filesystem::path &dir) {
    std::error_code ec;
    if (std::filesystem::exists(dir, ec)) {
        return;
    }
    std::filesystem::create_directories(dir, ec);
}

std::unique_ptr<pksm::PKX> MakePKXFromEntry(const BankEntry &entry) {
    if (entry.gen == pksm::Generation::UNUSED) {
        return nullptr;
    }

    switch (entry.gen) {
        case pksm::Generation::ONE: {
            const uint8_t jpEnd = entry.data[pksm::PK1::JP_LENGTH_WITH_NAMES - 1];
            const size_t len = (jpEnd == 0x50 || jpEnd == 0) ? pksm::PK1::JP_LENGTH_WITH_NAMES : pksm::PK1::INT_LENGTH_WITH_NAMES;
            return pksm::PKX::getPKM<pksm::Generation::ONE>(const_cast<uint8_t*>(entry.data), len);
        }
        case pksm::Generation::TWO: {
            const uint8_t jpEnd = entry.data[pksm::PK2::JP_LENGTH_WITH_NAMES - 1];
            const size_t len = (jpEnd == 0x50 || jpEnd == 0) ? pksm::PK2::JP_LENGTH_WITH_NAMES : pksm::PK2::INT_LENGTH_WITH_NAMES;
            return pksm::PKX::getPKM<pksm::Generation::TWO>(const_cast<uint8_t*>(entry.data), len);
        }
        case pksm::Generation::THREE:
            return pksm::PKX::getPKM<pksm::Generation::THREE>(const_cast<uint8_t*>(entry.data), pksm::PK3::BOX_LENGTH);
        case pksm::Generation::FOUR:
            return pksm::PKX::getPKM<pksm::Generation::FOUR>(const_cast<uint8_t*>(entry.data), pksm::PK4::BOX_LENGTH);
        case pksm::Generation::FIVE:
            return pksm::PKX::getPKM<pksm::Generation::FIVE>(const_cast<uint8_t*>(entry.data), pksm::PK5::BOX_LENGTH);
        case pksm::Generation::SIX:
            return pksm::PKX::getPKM<pksm::Generation::SIX>(const_cast<uint8_t*>(entry.data), pksm::PK6::BOX_LENGTH);
        case pksm::Generation::SEVEN:
            return pksm::PKX::getPKM<pksm::Generation::SEVEN>(const_cast<uint8_t*>(entry.data), pksm::PK7::BOX_LENGTH);
        case pksm::Generation::LGPE:
            return pksm::PKX::getPKM<pksm::Generation::LGPE>(const_cast<uint8_t*>(entry.data), pksm::PB7::BOX_LENGTH);
        case pksm::Generation::EIGHT:
            // padding[0] == 0x01 indicates PA8 (Legends: Arceus), which has a larger BOX_LENGTH.
            // getPKM<PA8> is used directly because GenToPkx<EIGHT> resolves to PK8, and
            // getPKM<PK8> would reject PA8's 0x168 length.
            if (entry.padding[0] == 0x01)
                return pksm::PKX::getPKM<pksm::PA8>(const_cast<uint8_t*>(entry.data), pksm::PA8::BOX_LENGTH);
            return pksm::PKX::getPKM<pksm::Generation::EIGHT>(const_cast<uint8_t*>(entry.data), pksm::PK8::BOX_LENGTH);
        case pksm::Generation::NINE:
            // Both PK9 (SV) and PA9 (Z-A) have BOX_LENGTH=0x148 and are binary-compatible.
            // GenToPkx<NINE> = PK9. PA9-specific fields (e.g., IsAlpha at 0x23) are preserved
            // in the raw bytes even when read back as PK9.
            return pksm::PKX::getPKM<pksm::Generation::NINE>(const_cast<uint8_t*>(entry.data), pksm::PK9::BOX_LENGTH);
        default:
            return nullptr;
    }
}

void FillEntryEmpty(BankEntry &entry) {
    std::fill_n(reinterpret_cast<uint8_t*>(&entry), sizeof(BankEntry), 0xFF);
}

void CreateNewBank(const std::filesystem::path &bankPath, int boxes) {
    BankHeader header{};
    std::memcpy(header.magic, BANK_MAGIC, 8);
    header.version = BANK_VERSION;
    header.boxes = static_cast<uint32_t>(std::max(0, boxes));

    std::ofstream out(bankPath.string(), std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        throw std::runtime_error("Failed to create bank file: " + bankPath.string());
    }

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    const size_t entryCount = static_cast<size_t>(header.boxes) * SLOTS_PER_BOX;
    BankEntry empty{};
    FillEntryEmpty(empty);
    for (size_t i = 0; i < entryCount; i++) {
        out.write(reinterpret_cast<const char*>(&empty), sizeof(empty));
    }

    out.flush();
    out.close();
}

void CreateNewBankNames(const std::filesystem::path &namesPath, int boxes) {
    nlohmann::json j = nlohmann::json::array();
    for (int i = 0; i < boxes; i++) {
        j.push_back(std::string("Storage ") + std::to_string(i + 1));
    }

    std::ofstream out(namesPath.string(), std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        throw std::runtime_error("Failed to create bank names file: " + namesPath.string());
    }

    out << j.dump();
    out.flush();
    out.close();
}

} // namespace

struct BankBoxDataProvider::BankEntry {
    ::BankEntry entry;
};

BankBoxDataProvider::BankBoxDataProvider(std::string bankName)
    : bankName(std::move(bankName)),
      rootPath(BANKS_ROOT),
      loaded(false),
      boxCount(0),
      entries(),
      boxNames() {}

BankBoxDataProvider::~BankBoxDataProvider() = default;

void BankBoxDataProvider::EnsureLoaded() const {
    if (loaded) {
        return;
    }

    loaded = true;

    try {
        const auto root = std::filesystem::path(rootPath);
        EnsureDirExists(root);

        const auto bankPath = BankFilePath(rootPath, bankName);
        const auto namesPath = NamesFilePath(rootPath, bankName);

        if (!std::filesystem::exists(bankPath)) {
            CreateNewBank(bankPath, 50);
        }

        std::ifstream in(bankPath.string(), std::ios::binary | std::ios::ate);
        if (!in.good()) {
            throw std::runtime_error("Failed to open bank file: " + bankPath.string());
        }

        const std::streamsize fileSize = in.tellg();
        if (fileSize < static_cast<std::streamsize>(sizeof(BankHeader))) {
            throw std::runtime_error("Bank file too small: " + bankPath.string());
        }
        in.seekg(0, std::ios::beg);

        BankHeader header{};
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!in.good()) {
            throw std::runtime_error("Failed to read bank header: " + bankPath.string());
        }

        if (std::memcmp(header.magic, BANK_MAGIC, 8) != 0) {
            throw std::runtime_error("Invalid bank magic: " + bankPath.string());
        }

        if (header.version != BANK_VERSION && header.version != BANK_VERSION_V3) {
            throw std::runtime_error("Unsupported bank version: " + std::to_string(header.version));
        }

        boxCount = static_cast<int>(header.boxes);
        if (boxCount < 0) {
            boxCount = 0;
        }

        const size_t expectedEntries = static_cast<size_t>(boxCount) * SLOTS_PER_BOX;
        entries.clear();
        entries.resize(expectedEntries);

        if (header.version == BANK_VERSION_V3) {
            // v3→v4 migration: read smaller v3 entries and expand data buffer.
            // v3 entries have 0x148 data; v4 has 0x168. Extra bytes are zero-filled.
            for (size_t i = 0; i < expectedEntries; i++) {
                BankEntryV3 rawV3{};
                in.read(reinterpret_cast<char*>(&rawV3), sizeof(rawV3));
                if (!in.good()) {
                    throw std::runtime_error("Failed to read v3 bank entry from: " + bankPath.string());
                }
                // Expand: copy gen, data (zero-extended), and preserve padding
                FillEntryEmpty(entries[i].entry);
                entries[i].entry.gen = rawV3.gen;
                std::copy_n(rawV3.data, sizeof(rawV3.data), entries[i].entry.data);
                std::copy_n(rawV3.padding, sizeof(rawV3.padding), entries[i].entry.padding);
            }
            in.close();

            // Upgrade file to v4 on disk
            pksm::utils::Logger::Info("[BankBoxDataProvider] Migrating bank from v3 to v4");
            SaveBank();
        } else {
            // v4: read entries with full 0x168 data buffer
            for (size_t i = 0; i < expectedEntries; i++) {
                ::BankEntry raw{};
                in.read(reinterpret_cast<char*>(&raw), sizeof(raw));
                if (!in.good()) {
                    throw std::runtime_error("Failed to read bank entry from: " + bankPath.string());
                }
                entries[i].entry = raw;
            }
            in.close();
        }

        if (!std::filesystem::exists(namesPath)) {
            CreateNewBankNames(namesPath, boxCount);
        }

        boxNames.clear();
        try {
            std::ifstream namesIn(namesPath.string());
            if (namesIn.good()) {
                nlohmann::json j;
                namesIn >> j;
                if (j.is_array()) {
                    for (const auto &v : j) {
                        if (v.is_string()) {
                            boxNames.push_back(v.get<std::string>());
                        }
                    }
                }
            }
        } catch (...) {
            boxNames.clear();
        }

        if (static_cast<int>(boxNames.size()) < boxCount) {
            for (int i = static_cast<int>(boxNames.size()); i < boxCount; i++) {
                boxNames.push_back(std::string("Storage ") + std::to_string(i + 1));
            }
        }

    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BankBoxDataProvider] Failed to load bank: ") + e.what());
        boxCount = 0;
        entries.clear();
        boxNames.clear();
    }
}

size_t BankBoxDataProvider::GetBoxCount(const pksm::saves::SaveData::Ref& saveData) const {
    (void)saveData;
    EnsureLoaded();
    return boxCount > 0 ? static_cast<size_t>(boxCount) : 0;
}

pksm::ui::BoxData BankBoxDataProvider::GetBoxData(const pksm::saves::SaveData::Ref& saveData, int boxIndex) const {
    (void)saveData;

    pksm::ui::BoxData boxData;
    EnsureLoaded();

    if ((boxIndex < 0) || (boxIndex >= boxCount)) {
        boxData.name = "Storage";
        boxData.resize(SLOTS_PER_BOX);
        return boxData;
    }

    boxData.name = (boxIndex < static_cast<int>(boxNames.size())) ? boxNames[boxIndex]
                                                                  : (std::string("Storage ") + std::to_string(boxIndex + 1));
    boxData.resize(SLOTS_PER_BOX);

    const int base = boxIndex * SLOTS_PER_BOX;
    for (int slot = 0; slot < SLOTS_PER_BOX; slot++) {
        const int idx = base + slot;
        if ((idx < 0) || (static_cast<size_t>(idx) >= entries.size())) {
            boxData.pokemon[slot] = pksm::ui::BoxPokemonData();
            continue;
        }

        auto pk = MakePKXFromEntry(entries[static_cast<size_t>(idx)].entry);
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

    return boxData;
}

bool BankBoxDataProvider::SetBoxData(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex,
    const pksm::ui::BoxData& boxData
) {
    (void)saveData;
    // BoxData only contains species/form/shiny per slot — not enough for full persistence.
    // We can update the box name though.
    return SetBoxName(boxIndex, boxData.name);
}

bool BankBoxDataProvider::SetPokemonData(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex,
    int slotIndex,
    const pksm::ui::BoxPokemonData& pokemonData
) {
    (void)saveData;
    // BoxPokemonData only has species/form/shiny — not enough to write a full Pokemon.
    // However, we can handle the "clear slot" case (species == 0).
    if (pokemonData.isEmpty()) {
        return ClearSlot(boxIndex, slotIndex);
    }
    pksm::utils::Logger::Error("[BankBoxDataProvider] SetPokemonData: use WritePokemon() for full PKX writes");
    return false;
}

std::unique_ptr<pksm::PKX> BankBoxDataProvider::GetPokemon(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex,
    int slotIndex
) const {
    (void)saveData;
    EnsureLoaded();

    if ((boxIndex < 0) || (slotIndex < 0) || (slotIndex >= SLOTS_PER_BOX)) {
        return nullptr;
    }

    if ((boxIndex < 0) || (boxIndex >= boxCount)) {
        return nullptr;
    }

    const int idx = (boxIndex * SLOTS_PER_BOX) + slotIndex;
    if ((idx < 0) || (static_cast<size_t>(idx) >= entries.size())) {
        return nullptr;
    }

    try {
        auto pk = MakePKXFromEntry(entries[static_cast<size_t>(idx)].entry);
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
        pksm::utils::Logger::Error(std::string("[BankBoxDataProvider] GetPokemon failed: ") + e.what());
        return nullptr;
    }
}

bool BankBoxDataProvider::WritePokemon(int boxIndex, int slotIndex, const pksm::PKX& pkx) {
    EnsureLoaded();

    if (boxIndex < 0 || boxIndex >= boxCount || slotIndex < 0 || slotIndex >= SLOTS_PER_BOX) {
        return false;
    }

    const size_t idx = static_cast<size_t>(boxIndex * SLOTS_PER_BOX + slotIndex);
    if (idx >= entries.size()) {
        return false;
    }

    try {
        // Clone and encrypt: bank stores encrypted PKX data, matching how save files work.
        // The read path (GetPokemon/GetBoxData) decrypts after loading.
        auto toStore = pkx.clone();
        if (!toStore->isEncrypted()) {
            toStore->encrypt();
        }

        auto& entry = entries[idx].entry;
        FillEntryEmpty(entry);

        entry.gen = toStore->generation();

        // Format tag in padding[0] distinguishes PKX sub-types within a generation.
        // PA8 (PLA) and PK8 (SWSH) both report Generation::EIGHT but have different sizes.
        // No RTTI available (-fno-rtti), so we use the virtual extension() method.
        const auto ext = toStore->extension();
        if (ext == ".pa8") {
            entry.padding[0] = 0x01;
        } else if (ext == ".pa9") {
            entry.padding[0] = 0x02;
        } else {
            entry.padding[0] = 0xFF;
        }

        const size_t copyLen = std::min(static_cast<size_t>(toStore->getLength()), BANK_DATA_SIZE);
        std::copy_n(toStore->rawData().data(), copyLen, entry.data);

        return SaveBank();
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BankBoxDataProvider] WritePokemon failed: ") + e.what());
        return false;
    }
}

bool BankBoxDataProvider::ClearSlot(int boxIndex, int slotIndex) {
    EnsureLoaded();

    if (boxIndex < 0 || boxIndex >= boxCount || slotIndex < 0 || slotIndex >= SLOTS_PER_BOX) {
        return false;
    }

    const size_t idx = static_cast<size_t>(boxIndex * SLOTS_PER_BOX + slotIndex);
    if (idx >= entries.size()) {
        return false;
    }

    FillEntryEmpty(entries[idx].entry);
    return SaveBank();
}

bool BankBoxDataProvider::SetBoxName(int boxIndex, const std::string& name) {
    EnsureLoaded();

    if (boxIndex < 0 || boxIndex >= boxCount) {
        return false;
    }

    if (static_cast<size_t>(boxIndex) < boxNames.size()) {
        boxNames[boxIndex] = name;
    }

    return SaveNames();
}

bool BankBoxDataProvider::SaveBank() const {
    try {
        const auto bankPath = BankFilePath(rootPath, bankName);
        EnsureDirExists(std::filesystem::path(rootPath));

        BankHeader header{};
        std::memcpy(header.magic, BANK_MAGIC, 8);
        header.version = BANK_VERSION;
        header.boxes = static_cast<uint32_t>(std::max(0, boxCount));

        std::ofstream out(bankPath.string(), std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            pksm::utils::Logger::Error("[BankBoxDataProvider] Failed to open bank for writing: " + bankPath.string());
            return false;
        }

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));

        for (size_t i = 0; i < entries.size(); i++) {
            out.write(reinterpret_cast<const char*>(&entries[i].entry), sizeof(::BankEntry));
        }

        out.flush();
        out.close();
        return true;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BankBoxDataProvider] SaveBank failed: ") + e.what());
        return false;
    }
}

bool BankBoxDataProvider::SaveNames() const {
    try {
        const auto namesPath = NamesFilePath(rootPath, bankName);
        EnsureDirExists(std::filesystem::path(rootPath));

        nlohmann::json j = nlohmann::json::array();
        for (const auto& name : boxNames) {
            j.push_back(name);
        }

        std::ofstream out(namesPath.string(), std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            pksm::utils::Logger::Error("[BankBoxDataProvider] Failed to open names for writing: " + namesPath.string());
            return false;
        }

        out << j.dump();
        out.flush();
        out.close();
        return true;
    } catch (const std::exception &e) {
        pksm::utils::Logger::Error(std::string("[BankBoxDataProvider] SaveNames failed: ") + e.what());
        return false;
    }
}