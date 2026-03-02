#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <pu/Plutonium>

#include "data/providers/interfaces/IBoxDataProvider.hpp"
#include "data/providers/interfaces/ISaveDataAccessor.hpp"
#include "gui/shared/components/AnimatedBackground.hpp"
#include "gui/shared/components/BaseLayout.hpp"
#include "gui/shared/components/BoxGrid.hpp"
#include "gui/shared/components/HelpFooter.hpp"
#include "gui/shared/components/HelpOverlay.hpp"
#include "gui/shared/components/PokemonBox.hpp"
#include "gui/shared/components/PokemonSummaryOverlay.hpp"
#include "gui/shared/interfaces/IHelpProvider.hpp"
#include "input/ButtonInputHandler.hpp"
#include "input/directional/DirectionalInputHandler.hpp"
#include "input/visual-feedback/FocusManager.hpp"
#include "input/visual-feedback/SelectionManager.hpp"

namespace pksm::layout
{

    class StorageScreen : public BaseLayout
    {
    public:
        StorageScreen(
            std::function<void()> onBack,
            std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
            std::function<void()> onHideOverlay,
            ISaveDataAccessor::Ref saveDataAccessor,
            IBoxDataProvider::Ref boxDataProvider,
            IBoxDataProvider::Ref bankBoxDataProvider);
        PU_SMART_CTOR(StorageScreen)
        ~StorageScreen();

        // Public method to load/refresh box data
        void LoadBoxData();

    private:
        enum class ActiveBox
        {
            Bank,
            Save
        };

        pu::ui::elm::TextBlock::Ref headerText;
        pu::ui::elm::Element::Ref background;
        pu::ui::Color bgColor = pu::ui::Color(5, 171, 49, 255);
        std::function<void()> onBack;
        pksm::ui::PokemonBox::Ref pokemonBankBox;
        pksm::ui::PokemonBox::Ref pokemonSaveBox;
        ActiveBox activeBox = ActiveBox::Save;
        ISaveDataAccessor::Ref saveDataAccessor;
        IBoxDataProvider::Ref boxDataProvider;
        IBoxDataProvider::Ref bankBoxDataProvider;

        bool isSummaryOverlayVisible = false;

        // Held Pokemon state for pick-up/place-down operations.
        // Uses deferred writes: no disk I/O until the user places on an empty slot.
        // Cancel restores all visuals with zero data loss.
        struct HeldPokemon {
            std::unique_ptr<pksm::PKX> pkx;         // Currently held PKX (changes on each carry-swap)
            IBoxDataProvider::Ref originalProvider;   // Provider of the original pick-up source
            int originalBox;                          // Original source box index
            int originalSlot;                         // Original source slot index
            bool originalFromBank;                    // Whether original source was bank side
            pksm::ui::BoxPokemonData originalVisual;  // Visual data at original source (for cancel)
        };

        // Pending writes accumulated during a carry-swap chain.
        // Each records what PKX to write at a slot and the previous visual for undo.
        struct DeferredWrite {
            IBoxDataProvider::Ref provider;
            int boxIndex;
            int slotIndex;
            bool isBank;
            std::unique_ptr<pksm::PKX> pkx;           // PKX to write at this slot on commit
            pksm::ui::BoxPokemonData previousVisual;   // Visual before this write (for cancel)
        };

        std::optional<HeldPokemon> heldPokemon;
        std::vector<DeferredWrite> deferredWrites;

        // Floating sprite showing the held Pokemon over the cursor
        pu::ui::elm::Image::Ref heldPokemonImage;

        // Layout constants
        static constexpr pu::i32 HEADER_TOP_MARGIN = 35;
        static constexpr pu::i32 SIDE_MARGIN = 70;
        static constexpr pu::i32 BOX_GRID_SIDE_MARGIN = 80;   // Margin from left edge
        static constexpr pu::i32 SAVE_BOX_SIDE_MARGIN = 1000; // Margin from left edge (right side, moved further right)
        static constexpr pu::i32 BOX_GRID_TOP_MARGIN = 120;   // Margin from top
        static constexpr pu::i32 BOX_ITEM_SIZE = 124;         // Size of each box item

        // Focus and selection management
        pksm::input::FocusManager::Ref storageScreenFocusManager;
        pksm::input::SelectionManager::Ref storageScreenSelectionManager;
        pksm::input::FocusManager::Ref pokemonBankBoxFocusManager;
        pksm::input::SelectionManager::Ref pokemonBankBoxSelectionManager;
        pksm::input::FocusManager::Ref pokemonSaveBoxFocusManager;
        pksm::input::SelectionManager::Ref pokemonSaveBoxSelectionManager;

        // Input handlers
        pksm::input::DirectionalInputHandler pokemonBoxDirectionalHandler;
        pksm::input::ButtonInputHandler buttonHandler;

        void OnInput(u64 down, u64 up, u64 held);
        void InitializePokemonBoxes();
        void InitializeFocusManagement();
        void SetActiveBox(ActiveBox box);

        // Pick-up/place-down operations
        void PickUp();
        void PlaceDown();
        void CancelPickUp();
        void UpdateHelpFooter();
        void UpdateHeldPokemonImage();

        // Override BaseLayout methods
        std::vector<pksm::ui::HelpItem> GetHelpOverlayItems() const override;
        void OnHelpOverlayShown() override;
        void OnHelpOverlayHidden() override;
    };

} // namespace pksm::layout