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
#include "pksmcore/pkx/PK8.hpp"
#include "pksmcore/pkx/PKX.hpp"
#include "utils/Logger.hpp"

namespace {

static constexpr const char *BANKS_ROOT = "sdmc:/switch/PKSM/banks";
static constexpr const char *BANK_MAGIC = "PKSMBANK";
static constexpr uint32_t BANK_VERSION = 3;
static constexpr int SLOTS_PER_BOX = 30;

struct BankHeader {
    char magic[8];
    uint32_t version;
    uint32_t boxes;
};
static_assert(sizeof(BankHeader) == 16);

struct BankEntry {
    pksm::Generation gen;
    uint8_t data[0x148];
    uint8_t padding[4];
};
static_assert(sizeof(BankEntry) == 0x150);

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
            return pksm::PKX::getPKM<pksm::Generation::EIGHT>(const_cast<uint8_t*>(entry.data), pksm::PK8::BOX_LENGTH);
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

        if (header.version != BANK_VERSION) {
            throw std::runtime_error("Unsupported bank version: " + std::to_string(header.version));
        }

        boxCount = static_cast<int>(header.boxes);
        if (boxCount < 0) {
            boxCount = 0;
        }

        const size_t expectedEntries = static_cast<size_t>(boxCount) * SLOTS_PER_BOX;
        entries.clear();
        entries.resize(expectedEntries);

        for (size_t i = 0; i < expectedEntries; i++) {
            ::BankEntry raw{};
            in.read(reinterpret_cast<char*>(&raw), sizeof(raw));
            if (!in.good()) {
                throw std::runtime_error("Failed to read bank entry from: " + bankPath.string());
            }
            entries[i].entry = raw;
        }

        in.close();

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
    (void)boxIndex;
    (void)boxData;
    pksm::utils::Logger::Error("[BankBoxDataProvider] SetBoxData not implemented yet");
    return false;
}

bool BankBoxDataProvider::SetPokemonData(
    const pksm::saves::SaveData::Ref& saveData,
    int boxIndex,
    int slotIndex,
    const pksm::ui::BoxPokemonData& pokemonData
) {
    (void)saveData;
    (void)boxIndex;
    (void)slotIndex;
    (void)pokemonData;
    pksm::utils::Logger::Error("[BankBoxDataProvider] SetPokemonData not implemented yet");
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