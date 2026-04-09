#pragma once

#include <functional>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

#include <pu/Plutonium>

#include "data/emulator/EmulatorGameCatalog.hpp"
#include "gui/shared/UIConstants.hpp"
#include "gui/shared/components/AnimatedBackground.hpp"
#include "gui/shared/components/BaseLayout.hpp"
#include "gui/shared/components/FocusableMenu.hpp"
#include "input/ButtonInputHandler.hpp"
#include "input/visual-feedback/FocusManager.hpp"

namespace pksm
{
    namespace layout
    {

        class EmulatorFileSelectScreen : public BaseLayout
        {
        public:
            EmulatorFileSelectScreen(
                std::function<void()> onBack,
                std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
                std::function<void()> onHideOverlay,
                const std::string &dataJsonPath = "romfs:/gfx/data/data.json");
            PU_SMART_CTOR(EmulatorFileSelectScreen)
            ~EmulatorFileSelectScreen() override = default;

        private:
            std::function<void()> onBack;

            pu::ui::elm::Element::Ref background;
            pu::ui::Color bgColor = pu::ui::Color(39, 66, 164, 255);

            pu::ui::elm::TextBlock::Ref headerText;
            pu::ui::elm::TextBlock::Ref hintText;

            pksm::ui::FocusableMenu::Ref gameList;
            pksm::ui::FocusableMenu::Ref fileList;

            pu::sdl2::TextureHandle::Ref gameEntryIcon;
            pu::sdl2::TextureHandle::Ref saveEntryIcon;
            std::unordered_map<std::string, pu::sdl2::TextureHandle::Ref> perGameIconCache;

            pksm::input::ButtonInputHandler buttonHandler;
            pksm::input::FocusManager::Ref focusManager;

            std::string dataJsonPath;
            std::vector<pksm::data::emulator::EmulatorGameEntry> emulatorGames;
            std::optional<pksm::data::emulator::EmulatorGameEntry> selectedGame;

            std::string currentDir;
            std::vector<std::string> currentEntries;

            static constexpr pu::i32 SIDE_MARGIN = 70;
            static constexpr pu::i32 HEADER_TOP_MARGIN = 40;
            static constexpr pu::i32 LIST_TOP = 140;

            static constexpr pu::i32 GAME_LIST_W = 520;
            static constexpr pu::i32 FILE_LIST_W = 700;
            static constexpr pu::i32 LIST_H = 8;
            static constexpr pu::i32 ITEM_H = 70;

            void OnInput(u64 down, u64 up, u64 held);

            void LoadGames();
            void UpdateGameList();
            void SelectGameByIndex(int idx);

            void ApplyIconsToFileList();

            void ResetBrowserToRoot();
            void RefreshFileList();
            void EnterSelectedEntry();
            void GoUpDirectory();
            void ApplySelectionToConfig();

            std::vector<pksm::ui::HelpItem> GetHelpOverlayItems() const override;
        };

    }

} // namespace pksm