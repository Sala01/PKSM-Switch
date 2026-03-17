#include "gui/screens/emulator-fileselect-screen/EmulatorFileSelectScreen.hpp"

#include <algorithm>
#include <filesystem>
#include <unordered_set>

#include "data/emulator/EmulatorSaveConfig.hpp"
#include "utils/NotificationManager.hpp"

namespace pksm {
namespace layout {

namespace {

std::string JoinPath(const std::string& base, const std::string& name) {
    if (base.empty()) {
        return name;
    }
    if (!base.empty() && base.back() == '/') {
        return base + name;
    }
    return base + "/" + name;
}

std::string ParentDir(const std::string& path) {
    // std::filesystem::path does not reliably handle the "sdmc:/" scheme, so do it manually.
    static const std::string kRoot = "sdmc:/";
    if (path.rfind(kRoot, 0) != 0) {
        try {
            std::filesystem::path p(path);
            auto parent = p.parent_path();
            if (parent.empty()) {
                return path;
            }
            auto s = parent.string();
            if (!s.empty() && s.back() != '/') {
                s.push_back('/');
            }
            return s;
        } catch (...) {
            return path;
        }
    }

    std::string p = path;
    while (p.size() > kRoot.size() && !p.empty() && p.back() == '/') {
        p.pop_back();
    }
    if (p.size() <= kRoot.size()) {
        return kRoot;
    }

    const auto last = p.find_last_of('/');
    if (last == std::string::npos || last < kRoot.size()) {
        return kRoot;
    }
    return p.substr(0, last + 1);
}

bool IsRootSdmc(const std::string& path) {
    return (path == "sdmc:/") || (path == "sdmc:" ) || (path == "sdmc://");
}

pu::sdl2::TextureHandle::Ref TryLoadIcon(const std::string& path) {
    try {
        auto image = pu::ui::render::LoadImage(path);
        if (image) {
            return pu::sdl2::TextureHandle::New(image);
        }
    } catch (...) {
    }
    return nullptr;
}

} // namespace

EmulatorFileSelectScreen::EmulatorFileSelectScreen(
    std::function<void()> onBack,
    std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
    std::function<void()> onHideOverlay,
    const std::string& dataJsonPath
)
  : BaseLayout(onShowOverlay, onHideOverlay),
    onBack(onBack),
    buttonHandler(),
    dataJsonPath(dataJsonPath) {

    this->SetBackgroundColor(bgColor);
    background = ui::AnimatedBackground::New();
    this->Add(background);

    headerText = pu::ui::elm::TextBlock::New(SIDE_MARGIN, HEADER_TOP_MARGIN, "Emulator Saves");
    headerText->SetColor(pksm::ui::global::TEXT_WHITE);
    headerText->SetFont(pksm::ui::global::MakeHeavyFontName(pksm::ui::global::FONT_SIZE_TITLE));
    this->Add(headerText);

    hintText = pu::ui::elm::TextBlock::New(SIDE_MARGIN, HEADER_TOP_MARGIN + 70, "A: Assign/Unassign  B: Back/Up");
    hintText->SetColor(pksm::ui::global::TEXT_WHITE);
    hintText->SetFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_ACCOUNT_NAME));
    this->Add(hintText);

    gameEntryIcon = TryLoadIcon("romfs:/gfx/data/game_icons/lg_pikachu_icon.jpg");
    saveEntryIcon = TryLoadIcon("romfs:/gfx/ui/beastball.png");
    if (!saveEntryIcon) {
        saveEntryIcon = gameEntryIcon;
    }

    gameList = pksm::ui::FocusableMenu::New(
        SIDE_MARGIN,
        LIST_TOP,
        GAME_LIST_W,
        pu::ui::Color(0, 0, 0, 200),
        pu::ui::Color(0, 150, 255, 255),
        ITEM_H,
        LIST_H
    );
    gameList->SetOnSelect([this]() {
        SelectGameByIndex(gameList->GetSelectedIndex());
        fileList->RequestFocus();
    });

    fileList = pksm::ui::FocusableMenu::New(
        SIDE_MARGIN + GAME_LIST_W + 20,
        LIST_TOP,
        FILE_LIST_W,
        pu::ui::Color(0, 0, 0, 200),
        pu::ui::Color(0, 150, 255, 255),
        ITEM_H,
        LIST_H
    );
    fileList->SetOnSelect([this]() {
        EnterSelectedEntry();
    });

    this->Add(gameList);
    this->Add(fileList);

    focusManager = pksm::input::FocusManager::New("EmulatorFileSelectScreen Manager");
    focusManager->SetActive(true);
    focusManager->RegisterFocusable(gameList);
    focusManager->RegisterFocusable(fileList);

    InitializeHelpFooter();

    buttonHandler.RegisterButton(HidNpadButton_B, nullptr, [this]() {
        if (fileList && fileList->IsFocused()) {
            if (gameList) {
                gameList->RequestFocus();
            }
            return;
        }

        if (this->onBack) {
            this->onBack();
        }
    });

    this->SetOnInput(std::bind(&EmulatorFileSelectScreen::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    LoadGames();
    ResetBrowserToRoot();

    if (gameList) {
        gameList->RequestFocus();
    }
}

void EmulatorFileSelectScreen::OnInput(u64 down, u64 up, u64 held) {
    if (HandleHelpInput(down)) {
        return;
    }

    buttonHandler.HandleInput(down, up, held);
}

void EmulatorFileSelectScreen::LoadGames() {
    emulatorGames = pksm::data::emulator::EmulatorGameCatalog::LoadFromDataJson(dataJsonPath);
    UpdateGameList();

    if (!emulatorGames.empty()) {
        SelectGameByIndex(0);
    }
}

void EmulatorFileSelectScreen::UpdateGameList() {
    std::vector<std::string> names;
    names.reserve(emulatorGames.size());
    for (const auto& g : emulatorGames) {
        names.push_back(g.name);
    }
    gameList->SetDataSource(names);

    if (gameEntryIcon && gameList) {
        auto& items = gameList->GetItems();
        const auto count = std::min(items.size(), emulatorGames.size());
        for (std::size_t i = 0; i < count; i++) {
            if (!items[i]) {
                continue;
            }

            const auto& g = emulatorGames[i];
            pu::sdl2::TextureHandle::Ref icon = gameEntryIcon;
            if (!g.iconPath.empty()) {
                const auto itc = perGameIconCache.find(g.iconPath);
                if (itc != perGameIconCache.end()) {
                    icon = itc->second ? itc->second : gameEntryIcon;
                } else {
                    auto loaded = TryLoadIcon(g.iconPath);
                    perGameIconCache.emplace(g.iconPath, loaded);
                    if (loaded) {
                        icon = loaded;
                    }
                }
            }

            if (icon) {
                items[i]->SetIcon(icon);
            }
        }
    }
}

void EmulatorFileSelectScreen::SelectGameByIndex(int idx) {
    if (idx < 0 || static_cast<size_t>(idx) >= emulatorGames.size()) {
        selectedGame.reset();
        return;
    }

    selectedGame = emulatorGames[static_cast<size_t>(idx)];

    if (hintText) {
        hintText->SetText("A: Assign/Unassign  B: Back/Up");
    }

    ApplyIconsToFileList();
}

void EmulatorFileSelectScreen::ResetBrowserToRoot() {
    currentDir = "sdmc:/";
    RefreshFileList();
}

void EmulatorFileSelectScreen::RefreshFileList() {
    currentEntries.clear();

    std::vector<std::string> display;
    display.reserve(128);

    // Always include up entry except at root
    if (!IsRootSdmc(currentDir)) {
        currentEntries.push_back("..");
        display.push_back(".. (up)");
    }

    try {
        std::vector<std::filesystem::directory_entry> entries;
        for (const auto& e : std::filesystem::directory_iterator(currentDir)) {
            entries.push_back(e);
        }

        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            const bool ad = a.is_directory();
            const bool bd = b.is_directory();
            if (ad != bd) {
                return ad > bd; // directories first
            }
            return a.path().filename().string() < b.path().filename().string();
        });

        for (const auto& e : entries) {
            const auto name = e.path().filename().string();
            if (name.empty()) {
                continue;
            }

            if (e.is_directory()) {
                currentEntries.push_back(JoinPath(currentDir, name));
                display.push_back(name + "/");
            } else if (e.is_regular_file()) {
                currentEntries.push_back(JoinPath(currentDir, name));
                display.push_back(name);
            }
        }
    } catch (...) {
        // ignore
    }

    fileList->SetDataSource(display);
    ApplyIconsToFileList();
}

void EmulatorFileSelectScreen::ApplyIconsToFileList() {
    if (!fileList || !saveEntryIcon || !selectedGame.has_value() || selectedGame->titleId == 0) {
        return;
    }

    auto& items = fileList->GetItems();
    const auto count = std::min(items.size(), currentEntries.size());
    for (std::size_t i = 0; i < count; i++) {
        if (!items[i]) {
            continue;
        }
        items[i]->SetIcon(nullptr);
    }

    std::unordered_set<std::string> configured;
    const auto cfg = pksm::data::emulator::EmulatorSaveConfig::Load();
    const auto it = cfg.find(selectedGame->titleId);
    if (it != cfg.end()) {
        for (const auto& p : it->second.primary) {
            configured.insert(p);
        }
        for (const auto& kv : it->second.extra) {
            for (const auto& p : kv.second) {
                configured.insert(p);
            }
        }
    }

    if (configured.empty()) {
        return;
    }

    for (std::size_t i = 0; i < count; i++) {
        if (!items[i]) {
            continue;
        }

        const auto& entry = currentEntries[i];
        if (configured.count(entry) > 0) {
            items[i]->SetIcon(saveEntryIcon);
        }
    }
}

void EmulatorFileSelectScreen::EnterSelectedEntry() {
    if (!fileList) {
        return;
    }

    const int idx = fileList->GetSelectedIndex();
    if (idx < 0 || static_cast<size_t>(idx) >= currentEntries.size()) {
        return;
    }

    const std::string chosen = currentEntries[static_cast<size_t>(idx)];
    if (chosen == "..") {
        GoUpDirectory();
        return;
    }

    try {
        if (std::filesystem::is_directory(chosen)) {
            currentDir = chosen;
            if (!currentDir.empty() && currentDir.back() != '/') {
                currentDir.push_back('/');
            }
            RefreshFileList();
            return;
        }
    } catch (...) {
    }

    // otherwise: assign this file path to selected game/slot
    ApplySelectionToConfig();
}

void EmulatorFileSelectScreen::GoUpDirectory() {
    if (IsRootSdmc(currentDir)) {
        return;
    }

    currentDir = ParentDir(currentDir);
    if (currentDir.empty() || IsRootSdmc(currentDir)) {
        currentDir = "sdmc:/";
    }

    RefreshFileList();
}

void EmulatorFileSelectScreen::ApplySelectionToConfig() {
    if (!selectedGame.has_value() || selectedGame->titleId == 0) {
        pksm::utils::NotificationManager::Push("Emulator", "Select a game first.");
        return;
    }

    const int idx = fileList->GetSelectedIndex();
    if (idx < 0 || static_cast<size_t>(idx) >= currentEntries.size()) {
        return;
    }

    const std::string chosen = currentEntries[static_cast<size_t>(idx)];
    if (chosen.empty() || chosen == "..") {
        return;
    }

    // If it's a directory, we don't assign
    try {
        if (std::filesystem::is_directory(chosen)) {
            return;
        }
    } catch (...) {
    }

    auto cfg = pksm::data::emulator::EmulatorSaveConfig::Load();

    auto& sel = cfg[selectedGame->titleId];

    auto eraseFromVec = [&](std::vector<std::string>& v) {
        const auto it = std::remove(v.begin(), v.end(), chosen);
        const bool removed = (it != v.end());
        v.erase(it, v.end());
        return removed;
    };

    bool removed = false;
    removed = eraseFromVec(sel.primary) || removed;
    for (auto it = sel.extra.begin(); it != sel.extra.end(); ) {
        removed = eraseFromVec(it->second) || removed;
        if (it->second.empty()) {
            it = sel.extra.erase(it);
        } else {
            ++it;
        }
    }

    if (!removed) {
        // auto-place into the first available slot: primary then slot1 -> slot20/
        if (sel.primary.empty()) {
            pksm::data::emulator::EmulatorSaveConfig::AddPrimaryPath(cfg, selectedGame->titleId, chosen);
        } else {
            bool placed = false;
            for (int i = 1; i <= 20; i++) {
                const std::string slot = "slot" + std::to_string(i);
                auto& v = sel.extra[slot];
                if (v.empty()) {
                    pksm::data::emulator::EmulatorSaveConfig::AddExtraPath(cfg, selectedGame->titleId, slot, chosen);
                    placed = true;
                    break;
                }
            }

            if (!placed) {
                pksm::utils::NotificationManager::Push("Emulator", "No free slots (20) for this game.");
                return;
            }
        }
    }

    if (!pksm::data::emulator::EmulatorSaveConfig::Save(cfg)) {
        pksm::utils::NotificationManager::Push("Emulator", "Failed to save configuration.");
        return;
    }

    pksm::utils::NotificationManager::Push("Emulator", removed ? "Removed save." : "Assigned save.");
    ApplyIconsToFileList();
}

std::vector<pksm::ui::HelpItem> EmulatorFileSelectScreen::GetHelpOverlayItems() const {
    return {
        {{{pksm::ui::global::ButtonGlyph::A}}, "Assign / Unassign"},
        {{{pksm::ui::global::ButtonGlyph::B}}, "Back / Up"},
        {{{pksm::ui::global::ButtonGlyph::Minus}}, "Help"}
    };
}

}

} // namespace pksm