#include "gui/screens/editor-screen/PokemonEditOverlay.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <pu/ui/elm/elm_Image.hpp>
#include <pu/ui/elm/elm_Rectangle.hpp>
#include <pu/ui/elm/elm_TextBlock.hpp>

#include "gui/shared/UIConstants.hpp"
#include "pksmcore/utils/io.hpp"
#include "pksmcore/utils/VersionTables.hpp"
#include "pksmcore/utils/i18n.hpp"
#include "utils/PokemonSpriteManager.hpp"

namespace pksm::ui {

namespace {

constexpr pu::ui::Color OVERLAY_BG = pu::ui::Color(7, 20, 56, 190);
constexpr pu::ui::Color PANEL_SHADOW = pu::ui::Color(0, 0, 0, 95);
constexpr pu::ui::Color PANEL_BORDER = pu::ui::Color(108, 130, 170, 255);
constexpr pu::ui::Color PANEL_BG = pu::ui::Color(214, 225, 242, 255);
constexpr pu::ui::Color HEADER_BG = pu::ui::Color(118, 28, 40, 255);
constexpr pu::ui::Color HEADER_HILITE = pu::ui::Color(160, 52, 68, 255);
constexpr pu::ui::Color HEADER_TXT = pu::ui::Color(242, 244, 252, 255);

constexpr pu::ui::Color LEFT_PANEL_BG = pu::ui::Color(238, 243, 251, 255);
constexpr pu::ui::Color RIGHT_PANEL_BG = pu::ui::Color(194, 208, 230, 200);
constexpr pu::ui::Color ROW_BG_A = pu::ui::Color(230, 239, 251, 255);
constexpr pu::ui::Color ROW_BG_B = pu::ui::Color(221, 233, 248, 255);
constexpr pu::ui::Color ROW_HL = pu::ui::Color(77, 147, 226, 110);
constexpr pu::ui::Color ROW_LABEL = pu::ui::Color(26, 41, 71, 255);
constexpr pu::ui::Color ROW_VALUE = pu::ui::Color(28, 37, 60, 255);
constexpr pu::ui::Color ROW_VALUE_DISABLED = pu::ui::Color(95, 108, 133, 255);

constexpr pu::ui::Color MENU_BTN = pu::ui::Color(136, 191, 226, 255);
constexpr pu::ui::Color MENU_BTN_ACTIVE = pu::ui::Color(75, 144, 215, 255);
constexpr pu::ui::Color MENU_BTN_SELECTED = pu::ui::Color(43, 113, 194, 255);
constexpr pu::ui::Color MENU_TXT = pu::ui::Color(16, 34, 77, 255);
constexpr pu::ui::Color MENU_TXT_SELECTED = pu::ui::Color(245, 251, 255, 255);

struct AbilityOption {
    pksm::Ability ability;
    u8 slot;
};

struct SpeciesFormKey {
    u16 species;
    u16 form;

    bool operator==(const SpeciesFormKey&) const = default;
};

struct SpeciesFormKeyHash {
    size_t operator()(const SpeciesFormKey& key) const {
        return (static_cast<size_t>(key.species) << 16) ^ static_cast<size_t>(key.form);
    }
};

struct LearnsetTable {
    std::unordered_map<u16, std::vector<pksm::Move>> byFormSpecies;
    std::unordered_map<SpeciesFormKey, std::vector<pksm::Move>, SpeciesFormKeyHash> bySpeciesForm;
};

void EnsureI18n() {
    static bool inited = false;
    if (!inited) {
        i18n::init(pksm::Language::ENG);
        inited = true;
    }
}

std::string SafeText(const std::string& value, const std::string& fallback = "-") {
    return value.empty() ? fallback : value;
}

std::string TrimCopy(std::string value) {
    const auto isSpace = [](const unsigned char c) {
        return std::isspace(c) != 0;
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](const char c) {
        return !isSpace(static_cast<unsigned char>(c));
    }));

    value.erase(std::find_if(value.rbegin(), value.rend(), [&](const char c) {
        return !isSpace(static_cast<unsigned char>(c));
    }).base(), value.end());

    return value;
}

bool ParseU16(const std::string& value, u16& out) {
    if (value.empty()) {
        return false;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 0);
    if (end == value.c_str() || *end != '\0' || parsed > std::numeric_limits<u16>::max()) {
        return false;
    }

    out = static_cast<u16>(parsed);
    return true;
}

std::vector<pksm::Move> ParseMoveList(const std::string& value) {
    std::vector<pksm::Move> moves;
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), ';', ',');

    std::stringstream listStream(normalized);
    std::string chunk;
    while (std::getline(listStream, chunk, ',')) {
        chunk = TrimCopy(std::move(chunk));
        if (chunk.empty()) {
            continue;
        }

        std::stringstream tokenStream(chunk);
        std::string token;
        while (tokenStream >> token) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(token.c_str(), &end, 0);
            if (end == token.c_str() || *end != '\0' || parsed > std::numeric_limits<u16>::max()) {
                continue;
            }
            moves.push_back(pksm::Move{static_cast<u16>(parsed)});
        }
    }

    std::sort(moves.begin(), moves.end());
    moves.erase(std::unique(moves.begin(), moves.end()), moves.end());
    return moves;
}

LearnsetTable LoadLearnsetTableFromPath(const std::string& path) {
    LearnsetTable table;

    FILE* input = fopen(path.c_str(), "rt");
    if (!input) {
        return table;
    }

    std::array<char, 16384> lineBuffer{};
    while (fgets(lineBuffer.data(), static_cast<int>(lineBuffer.size()), input) != nullptr) {
        std::string line = lineBuffer.data();
        const auto commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line.erase(commentPos);
        }

        line = TrimCopy(std::move(line));
        if (line.empty()) {
            continue;
        }

        const auto separatorPos = line.find('=');
        if (separatorPos == std::string::npos) {
            continue;
        }

        std::string key = TrimCopy(line.substr(0, separatorPos));
        std::string value = TrimCopy(line.substr(separatorPos + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        auto moves = ParseMoveList(value);
        if (moves.empty()) {
            continue;
        }

        const auto slashPos = key.find('/');
        if (slashPos != std::string::npos) {
            SpeciesFormKey speciesForm{};
            if (ParseU16(TrimCopy(key.substr(0, slashPos)), speciesForm.species) &&
                ParseU16(TrimCopy(key.substr(slashPos + 1)), speciesForm.form)) {
                table.bySpeciesForm[speciesForm] = std::move(moves);
            }
            continue;
        }

        u16 formSpecies = 0;
        if (ParseU16(key, formSpecies)) {
            table.byFormSpecies[formSpecies] = std::move(moves);
        }
    }

    fclose(input);
    return table;
}

const LearnsetTable& LearnsetTableForPath(const std::string& path) {
    static std::unordered_map<std::string, LearnsetTable> cache;

    auto it = cache.find(path);
    if (it == cache.end()) {
        it = cache.emplace(path, LoadLearnsetTableFromPath(path)).first;
    }
    return it->second;
}

std::vector<std::string> LearnsetPathCandidates(const pksm::PKX& pk) {
    const auto version = static_cast<u32>(pk.version());
    const auto generation = static_cast<u32>(pk.generation());

    std::vector<std::string> paths;
    paths.reserve(6);
    paths.push_back("sdmc:/switch/PKSM/learnsets/version_" + std::to_string(version) + ".txt");
    paths.push_back("sdmc:/switch/PKSM/learnsets/generation_" + std::to_string(generation) + ".txt");
    paths.push_back("sdmc:/switch/PKSM/learnsets/default.txt");
    paths.push_back("romfs:/learnsets/version_" + std::to_string(version) + ".txt");
    paths.push_back("romfs:/learnsets/generation_" + std::to_string(generation) + ".txt");
    paths.push_back("romfs:/learnsets/default.txt");
    return paths;
}

std::vector<pksm::Move> ExternalLearnsetMoves(const pksm::PKX& pk) {
    std::vector<pksm::Move> moves;

    const u16 formSpecies = pk.formSpecies();
    const SpeciesFormKey speciesForm = {
        static_cast<u16>(pk.species()),
        pk.alternativeForm(),
    };

    for (const auto& path : LearnsetPathCandidates(pk)) {
        if (!io::exists(path)) {
            continue;
        }

        const auto& table = LearnsetTableForPath(path);

        const auto byFormSpecies = table.byFormSpecies.find(formSpecies);
        if (byFormSpecies != table.byFormSpecies.end()) {
            moves.insert(moves.end(), byFormSpecies->second.begin(), byFormSpecies->second.end());
        }

        const auto bySpeciesForm = table.bySpeciesForm.find(speciesForm);
        if (bySpeciesForm != table.bySpeciesForm.end()) {
            moves.insert(moves.end(), bySpeciesForm->second.begin(), bySpeciesForm->second.end());
        }
    }

    std::sort(moves.begin(), moves.end());
    moves.erase(std::unique(moves.begin(), moves.end()), moves.end());
    return moves;
}

bool IsBlockedZMove(const pksm::Move move) {
    return move >= pksm::Move::BreakneckBlitzA && move <= pksm::Move::TwinkleTackleB;
}

void AppendSelectableMove(std::vector<pksm::Move>& options, const pksm::Move move) {
    if (move != pksm::Move::None && IsBlockedZMove(move)) {
        return;
    }
    options.push_back(move);
}

std::string StatShortName(const pksm::Stat stat) {
    switch (stat) {
        case pksm::Stat::HP:
            return "HP";
        case pksm::Stat::ATK:
            return "Atk";
        case pksm::Stat::DEF:
            return "Def";
        case pksm::Stat::SPATK:
            return "SpA";
        case pksm::Stat::SPDEF:
            return "SpD";
        case pksm::Stat::SPD:
            return "Spe";
    }
    return "Stat";
}

std::string GenderSuffix(const pksm::Gender gender) {
    if (gender == pksm::Gender::Male) {
        return " M";
    }
    if (gender == pksm::Gender::Female) {
        return " F";
    }
    return "";
}

std::string SpeciesName(const pksm::PKX& pk) {
    EnsureI18n();
    return SafeText(i18n::species(pksm::Language::ENG, pk.species()), "Species");
}

std::string NatureName(const pksm::Nature nature) {
    EnsureI18n();
    return SafeText(i18n::nature(pksm::Language::ENG, nature), "Nature");
}

std::string AbilityName(const pksm::Ability ability) {
    EnsureI18n();
    return SafeText(i18n::ability(pksm::Language::ENG, ability), "Ability");
}

bool SupportsHiddenAbilitySlot(const pksm::PKX& pk) {
    return pk.generation() >= pksm::Generation::FIVE;
}

u8 AbilityNumberForSlot(const u8 slot) {
    if (slot == 0) {
        return 1;
    }
    if (slot == 1) {
        return 2;
    }
    return 4;
}

std::string AbilitySlotSuffix(const pksm::PKX& pk) {
    if (pk.ability() == pksm::Ability::None) {
        return "";
    }

    const int slot = std::clamp(static_cast<int>(pk.abilityNumber() >> 1), 0, 2);
    if (slot == 0) {
        return " (1)";
    }
    if (slot == 1) {
        return " (2)";
    }
    return " (H)";
}

std::string ItemName(const pksm::PKX& pk) {
    EnsureI18n();
    if (pk.heldItem() == 0) {
        return "None";
    }
    return SafeText(i18n::item(pksm::Language::ENG, pk.heldItem()));
}

std::string MoveName(const pksm::PKX& pk, const u8 moveIndex) {
    EnsureI18n();
    const auto move = pk.move(moveIndex);
    if (move == pksm::Move::None) {
        return "-";
    }
    return SafeText(i18n::move(pksm::Language::ENG, move), "Move");
}

int PokerusState(const pksm::PKX& pk) {
    if (pk.pkrsDays() > 0) {
        return 1;  // Active
    }
    if (pk.pkrsStrain() > 0) {
        return 2;  // Cured
    }
    return 0;  // None
}

std::string PokerusStateText(const pksm::PKX& pk) {
    const int state = PokerusState(pk);
    if (state == 1) {
        return "Active";
    }
    if (state == 2) {
        return "Cured";
    }
    return "No";
}

void SetPokerusState(pksm::PKX& pk, const int state) {
    if (state <= 0) {
        pk.pkrsStrain(0);
        pk.pkrsDays(0);
        return;
    }
    if (state == 1) {
        pk.pkrsStrain(8);
        pk.pkrsDays(2);
        return;
    }
    pk.pkrsStrain(8);
    pk.pkrsDays(0);
}

u32 EvTotal(const pksm::PKX& pk) {
    return static_cast<u32>(pk.ev(pksm::Stat::HP)) + static_cast<u32>(pk.ev(pksm::Stat::ATK)) +
           static_cast<u32>(pk.ev(pksm::Stat::DEF)) + static_cast<u32>(pk.ev(pksm::Stat::SPATK)) +
           static_cast<u32>(pk.ev(pksm::Stat::SPDEF)) + static_cast<u32>(pk.ev(pksm::Stat::SPD));
}

int WrapIndex(int index, int size, int delta) {
    if (size <= 0) {
        return 0;
    }
    int next = (index + delta) % size;
    if (next < 0) {
        next += size;
    }
    return next;
}

template <typename T>
int IndexOf(const std::vector<T>& values, const T& needle) {
    auto it = std::find(values.begin(), values.end(), needle);
    if (it == values.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(values.begin(), it));
}

std::vector<int> BuildItemOptions(const pksm::PKX& pk) {
    std::vector<int> options;
    options.reserve(512);

    const auto& availableItems = pksm::VersionTables::availableItems(pk.version());
    for (const int itemId : availableItems) {
        if (itemId >= 0) {
            options.push_back(itemId);
        }
    }

    if (options.empty()) {
        options.push_back(0);
    }

    std::sort(options.begin(), options.end());
    options.erase(std::unique(options.begin(), options.end()), options.end());

    const int currentItem = static_cast<int>(pk.heldItem());
    if (IndexOf(options, currentItem) < 0) {
        options.push_back(currentItem);
        std::sort(options.begin(), options.end());
    }

    return options;
}

std::vector<pksm::Move> BuildMoveOptions(const pksm::PKX& pk) {
    std::vector<pksm::Move> options;
    options.reserve(1024);

    const auto speciesMoves = ExternalLearnsetMoves(pk);
    if (!speciesMoves.empty()) {
        for (const auto move : speciesMoves) {
            AppendSelectableMove(options, move);
        }
    } else {
        const auto& availableMoves = pksm::VersionTables::availableMoves(pk.version());
        for (const auto move : availableMoves) {
            // Keep game-valid moves and skip Z-moves from the selectable list.
            AppendSelectableMove(options, move);
        }
    }

    const pksm::Move noneMove{pksm::Move::None};
    AppendSelectableMove(options, noneMove);

    for (int i = 0; i < 4; i++) {
        const auto currentMove = pk.move(static_cast<u8>(i));
        AppendSelectableMove(options, currentMove);
        const auto relearnMove = pk.relearnMove(static_cast<u8>(i));
        AppendSelectableMove(options, relearnMove);
    }

    std::sort(options.begin(), options.end());
    options.erase(std::unique(options.begin(), options.end()), options.end());

    if (options.empty()) {
        options.push_back(noneMove);
    }

    return options;
}

template <typename AbilityReader>
void CollectAbilities(std::vector<AbilityOption>& options, AbilityReader&& readAbility, const u8 slots) {
    for (u8 slot = 0; slot < slots; slot++) {
        const auto ability = readAbility(slot);
        if (ability == pksm::Ability::None) {
            continue;
        }

        // Keep all non-empty slots (including duplicate IDs) so cycling follows
        // ability slot semantics exactly across generations.
        options.push_back({ability, slot});
    }
}

std::vector<AbilityOption> BuildAbilityOptions(const pksm::PKX& pk) {
    std::vector<AbilityOption> options;
    const std::array<pksm::Ability, 3> slotAbilities = {
        pk.abilities(0),
        pk.abilities(1),
        pk.abilities(2),
    };

    auto fallbackAbility = [&]() -> pksm::Ability {
        if (pk.ability() != pksm::Ability::None) {
            return pk.ability();
        }
        for (const auto ability : slotAbilities) {
            if (ability != pksm::Ability::None) {
                return ability;
            }
        }
        return pksm::Ability::None;
    };

    const bool includeHiddenSlot = SupportsHiddenAbilitySlot(pk);
    const u8 slotCount = includeHiddenSlot ? 3 : 2;

    for (u8 slot = 0; slot < slotCount; slot++) {
        auto ability = slotAbilities[slot];
        if (ability == pksm::Ability::None) {
            ability = fallbackAbility();
        }
        if (ability == pksm::Ability::None) {
            continue;
        }
        options.push_back({ability, slot});
    }

    if (options.empty() && pk.ability() != pksm::Ability::None) {
        options.push_back({pk.ability(), 0});
    }

    return options;
}

int CurrentAbilitySlot(const pksm::PKX& pk) {
    return std::clamp(static_cast<int>(pk.abilityNumber() >> 1), 0, 2);
}

const std::array<pksm::Stat, 6> kBattleStats = {
    pksm::Stat::HP,
    pksm::Stat::ATK,
    pksm::Stat::DEF,
    pksm::Stat::SPATK,
    pksm::Stat::SPDEF,
    pksm::Stat::SPD,
};

constexpr int kEvEditStep = 10;
constexpr int kLevelEditStep = 5;
constexpr int kFriendshipEditStep = 5;

}  // namespace

PokemonEditOverlay::PokemonEditOverlay(pu::i32 x, pu::i32 y, pu::i32 width, pu::i32 height)
    : pu::ui::Overlay(x, y, width, height, OVERLAY_BG) {
    this->SetRadius(0);
    this->SetMaxFadeAlpha(220);
    this->SetFadeAlphaVariation(22);
}

void PokemonEditOverlay::SetPokemon(std::unique_ptr<pksm::PKX> pokemon) {
    pk = std::move(pokemon);
    focusArea = FocusArea::Tabs;
    currentTab = Tab::Stats;
    selectedTab = 0;
    selectedRowsByTab = {0, 0, 0};
    selectedRow = 0;
    Rebuild();
}

std::unique_ptr<pksm::PKX> PokemonEditOverlay::TakePokemon() {
    return std::move(pk);
}

bool PokemonEditOverlay::HasPokemon() const {
    return pk != nullptr;
}

void PokemonEditOverlay::MoveUp() {
    if (!pk) {
        return;
    }

    if (focusArea == FocusArea::Tabs) {
        selectedTab = (selectedTab + 3) % 4;
    } else {
        const int rowCount = static_cast<int>(CurrentRowCount());
        if (rowCount > 0) {
            selectedRow = (selectedRow + rowCount - 1) % rowCount;
            selectedRowsByTab[static_cast<int>(currentTab)] = selectedRow;
        }
    }
    Rebuild();
}

void PokemonEditOverlay::MoveDown() {
    if (!pk) {
        return;
    }

    if (focusArea == FocusArea::Tabs) {
        selectedTab = (selectedTab + 1) % 4;
    } else {
        const int rowCount = static_cast<int>(CurrentRowCount());
        if (rowCount > 0) {
            selectedRow = (selectedRow + 1) % rowCount;
            selectedRowsByTab[static_cast<int>(currentTab)] = selectedRow;
        }
    }
    Rebuild();
}

void PokemonEditOverlay::MoveLeft() {
    if (!pk) {
        return;
    }

    if (focusArea == FocusArea::Tabs) {
        if (selectedTab < static_cast<int>(Tab::Save)) {
            currentTab = static_cast<Tab>(selectedTab);
            focusArea = FocusArea::Fields;
            selectedRow = selectedRowsByTab[selectedTab];
            ClampSelection();
        }
    } else {
        ApplyDelta(-1);
    }
    Rebuild();
}

void PokemonEditOverlay::MoveRight() {
    if (!pk) {
        return;
    }

    if (focusArea == FocusArea::Tabs) {
        if (selectedTab < static_cast<int>(Tab::Save)) {
            currentTab = static_cast<Tab>(selectedTab);
            focusArea = FocusArea::Fields;
            selectedRow = selectedRowsByTab[selectedTab];
            ClampSelection();
        }
    } else {
        ApplyDelta(1);
    }
    Rebuild();
}

PokemonEditOverlay::Result PokemonEditOverlay::Confirm() {
    if (!pk) {
        return Result::Cancel;
    }

    if (focusArea == FocusArea::Tabs) {
        if (selectedTab == static_cast<int>(Tab::Save)) {
            return Result::Save;
        }

        currentTab = static_cast<Tab>(selectedTab);
        selectedRow = selectedRowsByTab[selectedTab];
        focusArea = FocusArea::Fields;
        ClampSelection();
        Rebuild();
        return Result::None;
    }

    focusArea = FocusArea::Tabs;
    selectedTab = static_cast<int>(currentTab);
    Rebuild();
    return Result::None;
}

PokemonEditOverlay::Result PokemonEditOverlay::Cancel() {
    return Result::Cancel;
}

void PokemonEditOverlay::ClampSelection() {
    const int rowCount = static_cast<int>(CurrentRowCount());
    if (rowCount <= 0) {
        selectedRow = 0;
        return;
    }
    selectedRow = std::clamp(selectedRow, 0, rowCount - 1);
    selectedRowsByTab[static_cast<int>(currentTab)] = selectedRow;
}

size_t PokemonEditOverlay::CurrentRowCount() const {
    return BuildRows().size();
}

std::vector<PokemonEditOverlay::FieldRow> PokemonEditOverlay::BuildRows() const {
    std::vector<FieldRow> rows;
    if (!pk) {
        return rows;
    }

    if (currentTab == Tab::Stats) {
        rows.push_back({"Level", std::to_string(pk->level()), true});
        rows.push_back({"Nature", NatureName(pk->nature()), true});
        rows.push_back({"Ability", AbilityName(pk->ability()) + AbilitySlotSuffix(*pk), true});
        rows.push_back({"Item", ItemName(*pk), true});
        rows.push_back({"Shiny", pk->shiny() ? "Yes" : "No", true});

        for (const auto stat : kBattleStats) {
            rows.push_back({"EV " + StatShortName(stat), std::to_string(pk->ev(stat)), true});
        }
        rows.push_back({"EV Total", std::to_string(EvTotal(*pk)) + "/" + std::to_string(pk->maxEVTotal()), false});

        for (const auto stat : kBattleStats) {
            rows.push_back({"IV " + StatShortName(stat), std::to_string(pk->iv(stat)), true});
        }

        return rows;
    }

    if (currentTab == Tab::Moves) {
        rows.push_back({"Move 1", MoveName(*pk, 0), true});
        rows.push_back({"Move 2", MoveName(*pk, 1), true});
        rows.push_back({"Move 3", MoveName(*pk, 2), true});
        rows.push_back({"Move 4", MoveName(*pk, 3), true});
        return rows;
    }

    if (currentTab == Tab::Misc) {
        rows.push_back({"Pokerus", PokerusStateText(*pk), true});
        rows.push_back({"Friendship", std::to_string(pk->currentFriendship()), true});
        rows.push_back({"OT", SafeText(pk->otName()), false});
        rows.push_back({"Nickname", SafeText(pk->nickname()), false});
        return rows;
    }

    return rows;
}

void PokemonEditOverlay::ApplyDelta(const int delta) {
    if (!pk || delta == 0) {
        return;
    }

    if (currentTab == Tab::Stats) {
        if (selectedRow == 0) {
            const int step = (delta > 0 ? kLevelEditStep : -kLevelEditStep);
            const int nextLevel = std::clamp(static_cast<int>(pk->level()) + step, 1, 100);
            pk->level(static_cast<u8>(nextLevel));
            return;
        }

        if (selectedRow == 1) {
            int nextNature = static_cast<int>(static_cast<u8>(pk->nature()));
            nextNature = WrapIndex(nextNature, 25, delta);
            pk->nature(static_cast<pksm::Nature>(nextNature));
            return;
        }

        if (selectedRow == 2) {
            auto options = BuildAbilityOptions(*pk);
            if (!options.empty()) {
                int currentIndex = -1;

                const int currentSlot = CurrentAbilitySlot(*pk);
                for (size_t i = 0; i < options.size(); i++) {
                    if (options[i].slot == currentSlot) {
                        currentIndex = static_cast<int>(i);
                        break;
                    }
                }
                if (currentIndex < 0) {
                    for (size_t i = 0; i < options.size(); i++) {
                        if (options[i].ability == pk->ability()) {
                            currentIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
                if (currentIndex < 0) {
                    currentIndex = 0;
                }

                const int nextIndex = WrapIndex(currentIndex, static_cast<int>(options.size()), delta);
                const auto selected = options[nextIndex];
                pk->setAbility(selected.slot);
                if (pk->ability() != selected.ability) {
                    pk->ability(selected.ability);
                }

                // Keep slot metadata aligned even if a concrete PKX uses a no-op setAbility.
                if ((pk->abilityNumber() >> 1) != selected.slot) {
                    pk->abilityNumber(AbilityNumberForSlot(selected.slot));
                }
            }
            return;
        }

        if (selectedRow == 3) {
            auto options = BuildItemOptions(*pk);
            int currentIndex = IndexOf(options, static_cast<int>(pk->heldItem()));
            if (currentIndex < 0) {
                currentIndex = 0;
            }
            const int nextIndex = WrapIndex(currentIndex, static_cast<int>(options.size()), delta);
            const int nextItem = options[nextIndex];
            pk->heldItem(static_cast<u16>(nextItem));
            return;
        }

        if (selectedRow == 4) {
            pk->shiny(!pk->shiny());
            return;
        }

        if (selectedRow >= 5 && selectedRow <= 10) {
            const auto stat = kBattleStats[static_cast<size_t>(selectedRow - 5)];
            const int currentValue = static_cast<int>(pk->ev(stat));
            const u32 total = EvTotal(*pk);
            const u32 other = total - static_cast<u32>(currentValue);
            const u32 maxTotal = pk->maxEVTotal();
            const int maxSingle = static_cast<int>(pk->maxEV());
            const int maxAllowedByTotal = static_cast<int>(other >= maxTotal ? 0 : (maxTotal - other));
            const int maxAllowed = std::max(0, std::min(maxSingle, maxAllowedByTotal));

            const int step = (delta > 0 ? kEvEditStep : -kEvEditStep);
            const int nextValue = std::clamp(currentValue + step, 0, maxAllowed);
            pk->ev(stat, static_cast<u16>(nextValue));
            return;
        }

        if (selectedRow == 11) {
            return;
        }

        if (selectedRow >= 12 && selectedRow <= 17) {
            const auto stat = kBattleStats[static_cast<size_t>(selectedRow - 12)];
            const int currentValue = static_cast<int>(pk->iv(stat));
            const int maxIv = static_cast<int>(pk->maxIV());
            int nextValue = currentValue + (delta > 0 ? 1 : -1);

            if (nextValue > maxIv) {
                nextValue = 0;
            } else if (nextValue < 0) {
                nextValue = maxIv;
            }

            pk->iv(stat, static_cast<u8>(nextValue));
            return;
        }
    }

    if (currentTab == Tab::Moves) {
        if (selectedRow >= 0 && selectedRow <= 3) {
            auto options = BuildMoveOptions(*pk);
            int currentIndex = IndexOf(options, pk->move(static_cast<u8>(selectedRow)));
            if (currentIndex < 0) {
                currentIndex = 0;
            }

            const int nextIndex = WrapIndex(currentIndex, static_cast<int>(options.size()), delta);
            pk->move(static_cast<u8>(selectedRow), options[nextIndex]);
        }
        return;
    }

    if (currentTab == Tab::Misc) {
        if (selectedRow == 0) {
            int nextState = PokerusState(*pk) + delta;
            if (nextState < 0) {
                nextState = 2;
            } else if (nextState > 2) {
                nextState = 0;
            }
            SetPokerusState(*pk, nextState);
            return;
        }

        if (selectedRow == 1) {
            const int step = (delta > 0 ? kFriendshipEditStep : -kFriendshipEditStep);
            int friendship = static_cast<int>(pk->currentFriendship()) + step;
            friendship = std::clamp(friendship, 0, 255);
            pk->currentFriendship(static_cast<u8>(friendship));
        }
    }
}

void PokemonEditOverlay::Rebuild() {
    this->Clear();

    if (!pk) {
        return;
    }

    const std::string fontTitle = pksm::ui::global::MakeHeavyFontName(34);
    const std::string fontBody = pksm::ui::global::MakeMediumFontName(24);
    const std::string fontLabel = pksm::ui::global::MakeHeavyFontName(24);
    const std::string fontMenu = pksm::ui::global::MakeHeavyFontName(30);
    const std::string fontHint = pksm::ui::global::MakeMediumFontName(19);

    const pu::i32 panelX = 24;
    const pu::i32 panelY = 20;
    const pu::i32 panelW = this->GetWidth() - 48;
    const pu::i32 panelH = this->GetHeight() - 40;

    auto panelShadow = pu::ui::elm::Rectangle::New(panelX + 8, panelY + 10, panelW, panelH, PANEL_SHADOW, 18);
    this->Add(panelShadow);

    auto panelBorder = pu::ui::elm::Rectangle::New(panelX, panelY, panelW, panelH, PANEL_BORDER, 18);
    this->Add(panelBorder);

    auto panel = pu::ui::elm::Rectangle::New(panelX + 2, panelY + 2, panelW - 4, panelH - 4, PANEL_BG, 16);
    this->Add(panel);

    const pu::i32 headerX = panelX + 18;
    const pu::i32 headerY = panelY + 12;
    const pu::i32 headerW = panelW - 36;
    const pu::i32 headerH = 72;

    auto header = pu::ui::elm::Rectangle::New(headerX, headerY, headerW, headerH, HEADER_BG, 10);
    this->Add(header);

    auto headerHilite = pu::ui::elm::Rectangle::New(headerX + 4, headerY + 4, headerW - 8, 16, HEADER_HILITE, 8);
    this->Add(headerHilite);

    const std::string shinyMark = pk->shiny() ? " *" : "";
    auto speciesText = pu::ui::elm::TextBlock::New(
        headerX + 20,
        headerY + 18,
        SpeciesName(*pk) + GenderSuffix(pk->gender()) + shinyMark
    );
    speciesText->SetFont(fontTitle);
    speciesText->SetColor(HEADER_TXT);
    this->Add(speciesText);

    auto levelText = pu::ui::elm::TextBlock::New(
        0,
        headerY + 22,
        "Level " + std::to_string(pk->level())
    );
    levelText->SetFont(fontLabel);
    levelText->SetColor(HEADER_TXT);
    levelText->SetX(headerX + headerW - levelText->GetWidth() - 28);
    this->Add(levelText);

    const pu::i32 leftX = panelX + 36;
    const pu::i32 leftY = headerY + headerH + 18;
    const pu::i32 leftW = panelW - 432;
    const pu::i32 leftH = panelH - (leftY - panelY) - 32;

    auto leftPanel = pu::ui::elm::Rectangle::New(leftX, leftY, leftW, leftH, LEFT_PANEL_BG, 10);
    this->Add(leftPanel);

    const pu::i32 rightPanelX = panelX + panelW - 292;
    const pu::i32 rightPanelY = leftY;
    const pu::i32 rightPanelW = 256;
    const pu::i32 rightPanelH = leftH;
    auto rightPanel = pu::ui::elm::Rectangle::New(rightPanelX, rightPanelY, rightPanelW, rightPanelH, RIGHT_PANEL_BG, 10);
    this->Add(rightPanel);

    const std::array<std::string, 3> tabTitles = {"Stats", "Moves", "Misc"};
    auto tabHeader = pu::ui::elm::TextBlock::New(leftX + 14, leftY + 8, tabTitles[static_cast<int>(currentTab)]);
    tabHeader->SetFont(fontTitle);
    tabHeader->SetColor(ROW_LABEL);
    this->Add(tabHeader);

    auto rows = BuildRows();
    const pu::i32 rowStartY = leftY + 58;
    const pu::i32 rowHeight = 44;
    const pu::i32 rowBottomY = leftY + leftH - 54;
    const int visibleRows = std::max(1, (rowBottomY - rowStartY) / rowHeight);

    int firstRow = 0;
    if (focusArea == FocusArea::Fields && static_cast<int>(rows.size()) > visibleRows) {
        firstRow = std::clamp(selectedRow - (visibleRows / 2), 0, static_cast<int>(rows.size()) - visibleRows);
    }

    const int rowLimit = std::min(static_cast<int>(rows.size()), firstRow + visibleRows);
    for (int rowIndex = firstRow; rowIndex < rowLimit; rowIndex++) {
        const pu::i32 rowY = rowStartY + (rowIndex - firstRow) * rowHeight;
        const bool selected = (focusArea == FocusArea::Fields) && (rowIndex == selectedRow);
        const auto& row = rows[static_cast<size_t>(rowIndex)];

        auto rowBg = pu::ui::elm::Rectangle::New(
            leftX + 8,
            rowY,
            leftW - 16,
            rowHeight - 2,
            (rowIndex % 2 == 0) ? ROW_BG_A : ROW_BG_B,
            8
        );
        this->Add(rowBg);

        if (selected) {
            auto hl = pu::ui::elm::Rectangle::New(leftX + 8, rowY, leftW - 16, rowHeight - 2, ROW_HL, 8);
            this->Add(hl);
        }

        auto label = pu::ui::elm::TextBlock::New(leftX + 16, rowY + 7, row.label);
        label->SetFont(fontLabel);
        label->SetColor(ROW_LABEL);
        this->Add(label);

        auto value = pu::ui::elm::TextBlock::New(leftX + leftW - 28, rowY + 7, row.value);
        value->SetFont(fontBody);
        value->SetColor(row.editable ? ROW_VALUE : ROW_VALUE_DISABLED);
        value->SetX(leftX + leftW - value->GetWidth() - 24);
        this->Add(value);
    }

    if (firstRow > 0) {
        auto upHint = pu::ui::elm::TextBlock::New(leftX + leftW - 42, rowStartY - 4, "^");
        upHint->SetFont(fontLabel);
        upHint->SetColor(ROW_LABEL);
        this->Add(upHint);
    }

    if (rowLimit < static_cast<int>(rows.size())) {
        auto downHint = pu::ui::elm::TextBlock::New(leftX + leftW - 42, rowBottomY - 4, "v");
        downHint->SetFont(fontLabel);
        downHint->SetColor(ROW_LABEL);
        this->Add(downHint);
    }

    const pu::i32 menuX = rightPanelX + 3;
    const pu::i32 spriteFrameY = leftY + 6;
    const pu::i32 spriteFrameW = 250;
    const pu::i32 spriteFrameH = 158;

    auto spriteFrame = pu::ui::elm::Rectangle::New(menuX, spriteFrameY, spriteFrameW, spriteFrameH, pu::ui::Color(224, 234, 246, 255), 10);
    this->Add(spriteFrame);

    const u16 formU16 = pk->alternativeForm();
    const u8 form = formU16 > 255 ? 0 : static_cast<u8>(formU16);
    auto spriteTexture = utils::PokemonSpriteManager::GetPokemonSprite(static_cast<u16>(pk->species()), form, pk->shiny());
    if (spriteTexture) {
        auto sprite = pu::ui::elm::Image::New(0, 0, spriteTexture);
        const pu::i32 spriteW = 128;
        const pu::i32 spriteH = 128;
        sprite->SetWidth(spriteW);
        sprite->SetHeight(spriteH);
        sprite->SetX(menuX + (spriteFrameW - spriteW) / 2);
        sprite->SetY(spriteFrameY + (spriteFrameH - spriteH) / 2);
        this->Add(sprite);
    } else {
        auto noSprite = pu::ui::elm::TextBlock::New(menuX + 64, spriteFrameY + 66, "No sprite");
        noSprite->SetFont(fontHint);
        noSprite->SetColor(ROW_LABEL);
        this->Add(noSprite);
    }

    const std::array<std::string, 4> menuLabels = {"STATS", "MOVES", "MISC", "SAVE"};
    const pu::i32 menuStartY = spriteFrameY + spriteFrameH + 20;
    const pu::i32 menuBtnH = 56;
    const pu::i32 menuBtnGap = 10;

    for (int i = 0; i < 4; i++) {
        const pu::i32 y = menuStartY + i * (menuBtnH + menuBtnGap);
        const bool tabActive = (i == static_cast<int>(currentTab));
        const bool tabSelected = (focusArea == FocusArea::Tabs) && (selectedTab == i);

        pu::ui::Color bg = MENU_BTN;
        pu::ui::Color fg = MENU_TXT;
        if (tabActive) {
            bg = MENU_BTN_ACTIVE;
        }
        if (tabSelected) {
            bg = MENU_BTN_SELECTED;
            fg = MENU_TXT_SELECTED;
        }

        auto button = pu::ui::elm::Rectangle::New(menuX, y, 250, menuBtnH, bg, 26);
        this->Add(button);

        auto text = pu::ui::elm::TextBlock::New(0, y + 12, menuLabels[i]);
        text->SetFont(fontMenu);
        text->SetColor(fg);
        text->SetX(menuX + (250 - text->GetWidth()) / 2);
        this->Add(text);
    }

    const std::string legalNote = (currentTab == Tab::Stats)
        ? "Abilities restricted to species/form"
        : (currentTab == Tab::Moves ? "Moves cycle through game-valid move list (3DS style)" : "");
    if (!legalNote.empty()) {
        auto note = pu::ui::elm::TextBlock::New(leftX, leftY + leftH - 54, legalNote);
        note->SetFont(fontHint);
        note->SetColor(ROW_VALUE_DISABLED);
        this->Add(note);
    }

    const std::string focusHint = (focusArea == FocusArea::Tabs)
        ? "Up/Down Menu  Left Enter  A Select"
        : "Up/Down Field  Left/Right Change  A Menu";
    auto hint = pu::ui::elm::TextBlock::New(leftX, panelY + panelH - 30, focusHint + "  B Cancel");
    hint->SetFont(fontHint);
    hint->SetColor(ROW_LABEL);
    this->Add(hint);
}

}  // namespace pksm::ui
