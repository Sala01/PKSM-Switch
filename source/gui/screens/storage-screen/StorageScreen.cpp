#include "gui/screens/storage-screen/StorageScreen.hpp"

#include <ctime>

#include "gui/screens/main-menu/sub-components/menu-grid/MenuButtonGrid.hpp"
#include "utils/Logger.hpp"

namespace pksm::layout {

StorageScreen::StorageScreen(
    std::function<void()> onBack,
    std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
    std::function<void()> onHideOverlay,
    ISaveDataAccessor::Ref saveDataAccessor,
    IBoxDataProvider::Ref boxDataProvider,
    IBoxDataProvider::Ref bankBoxDataProvider
)
  : BaseLayout(onShowOverlay, onHideOverlay),
    onBack(onBack),
    saveDataAccessor(saveDataAccessor),
    boxDataProvider(boxDataProvider),
    bankBoxDataProvider(bankBoxDataProvider),
    isSummaryOverlayVisible(false) {
    LOG_DEBUG("Initializing StorageScreen...");

    this->SetBackgroundColor(bgColor);
    background = ui::AnimatedBackground::New();
    this->Add(background);

    headerText = pu::ui::elm::TextBlock::New(SIDE_MARGIN, HEADER_TOP_MARGIN, "Storage");
    headerText->SetColor(pksm::ui::global::TEXT_WHITE);
    headerText->SetFont(pksm::ui::global::MakeHeavyFontName(pksm::ui::global::FONT_SIZE_TITLE));
    this->Add(headerText);

    // Initialize focus management
    InitializeFocusManagement();

    // Initialize BoxGrid
    InitializePokemonBoxes();

    // Initialize help footer
    InitializeHelpFooter();

    // A button: pick up or place down Pokemon
    buttonHandler.RegisterButton(HidNpadButton_A, nullptr, [this]() {
        if (isSummaryOverlayVisible) return;
        if (heldPokemon.has_value()) {
            PlaceDown();
        } else {
            PickUp();
        }
    });

    // B button: cancel pick-up if holding, otherwise go back
    buttonHandler.RegisterButton(HidNpadButton_B, nullptr, [this]() {
        if (heldPokemon.has_value()) {
            CancelPickUp();
            return;
        }
        LOG_DEBUG("B button pressed, returning to main menu");
        if (this->onBack) {
            this->onBack();
        }
    });

    buttonHandler.RegisterButton(HidNpadButton_X, nullptr, [this]() {
        if (isSummaryOverlayVisible || heldPokemon.has_value()) {
            return;
        }

        if (!this->saveDataAccessor) {
            return;
        }

        pksm::ui::PokemonBox::Ref targetBox;
        IBoxDataProvider::Ref provider;
        pksm::saves::SaveData::Ref saveData;
        if (activeBox == ActiveBox::Save) {
            targetBox = pokemonSaveBox;
            provider = this->boxDataProvider;
            saveData = this->saveDataAccessor->getCurrentSaveData();
            if (!provider || !saveData) {
                return;
            }
        } else {
            targetBox = pokemonBankBox;
            provider = this->bankBoxDataProvider;
            saveData = this->saveDataAccessor->getCurrentSaveData();
            if (!provider) {
                return;
            }
        }

        if (!targetBox) {
            return;
        }

        const int boxIndex = targetBox->GetCurrentBox();
        const int slotIndex = targetBox->GetSelectedSlot();
        if (slotIndex < 0) {
            return;
        }

        const auto slotData = targetBox->GetPokemonData(boxIndex, slotIndex);
        if (slotData.isEmpty()) {
            return;
        }

        auto pk = provider->GetPokemon(saveData, boxIndex, slotIndex);
        if (!pk) {
            return;
        }

        auto overlay = pksm::ui::PokemonSummaryOverlay::New(0, 0, GetWidth(), GetHeight());
        overlay->SetPokemon(std::move(pk));
        this->onShowOverlay(overlay);
        isSummaryOverlayVisible = true;

        if (pokemonBankBox) {
            pokemonBankBox->SetDisabled(true);
        }
        if (pokemonSaveBox) {
            pokemonSaveBox->SetDisabled(true);
        }
    });

    // Set initial help items
    UpdateHelpFooter();

    // Set up input handling
    this->SetOnInput(
        std::bind(&StorageScreen::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );

    LOG_DEBUG("StorageScreen initialization complete");
    PreRender();
}

void StorageScreen::InitializeFocusManagement() {
    LOG_DEBUG("Initializing focus and selection management...");

    // Initialize focus managers
    storageScreenFocusManager = pksm::input::FocusManager::New("StorageScreen Manager");
    storageScreenFocusManager->SetActive(true);  // since this is the root manager
    pokemonBankBoxFocusManager = pksm::input::FocusManager::New("PokemonBankBox Manager"); // bank box focus manager
    pokemonSaveBoxFocusManager = pksm::input::FocusManager::New("PokemonSaveBox Manager"); // save box focus manager

    // Initialize selection managers
    storageScreenSelectionManager = pksm::input::SelectionManager::New("StorageScreen Manager");
    storageScreenSelectionManager->SetActive(true);  // since this is the root manager
    pokemonBankBoxSelectionManager = pksm::input::SelectionManager::New("PokemonBankBox Manager"); // bank box selection manager
    pokemonSaveBoxSelectionManager = pksm::input::SelectionManager::New("PokemonSaveBox Manager"); // save box selection manager

    storageScreenFocusManager->RegisterChildManager(pokemonBankBoxFocusManager); // register bank box focus manager
    storageScreenFocusManager->RegisterChildManager(pokemonSaveBoxFocusManager); // register save box focus manager
    storageScreenSelectionManager->RegisterChildManager(pokemonBankBoxSelectionManager); // register bank box selection manager
    storageScreenSelectionManager->RegisterChildManager(pokemonSaveBoxSelectionManager); // register save box selection manager

    // Set up directional input handlers
    pokemonBoxDirectionalHandler.SetOnMoveLeft([this]() {
        if (activeBox == ActiveBox::Save) {
            SetActiveBox(ActiveBox::Bank);
        }
    });
    pokemonBoxDirectionalHandler.SetOnMoveRight([this]() {
        if (activeBox == ActiveBox::Bank) {
            SetActiveBox(ActiveBox::Save);
        }
    });

    LOG_DEBUG("Focus and selection management initialization complete");
}

void StorageScreen::InitializePokemonBoxes() {
    LOG_DEBUG("Initializing PokemonBoxes...");

    pokemonBankBox = pksm::ui::PokemonBox::New(
        BOX_GRID_SIDE_MARGIN,
        BOX_GRID_TOP_MARGIN,
        BOX_ITEM_SIZE,
        pokemonBankBoxFocusManager,
        pokemonBankBoxSelectionManager
    );
    this->Add(pokemonBankBox);
    pokemonBankBox->SetName("PokemonBankBox Element");
    pokemonBankBox->EstablishOwningRelationship();

    pokemonSaveBox = pksm::ui::PokemonBox::New(
        SAVE_BOX_SIDE_MARGIN,
        BOX_GRID_TOP_MARGIN,
        BOX_ITEM_SIZE,
        pokemonSaveBoxFocusManager,
        pokemonSaveBoxSelectionManager
    );
    this->Add(pokemonSaveBox);
    pokemonSaveBox->SetName("PokemonSaveBox Element");
    pokemonSaveBox->EstablishOwningRelationship();

    // Load box data for both Bank and Save
    LoadBoxData();

    pokemonBankBox->SetOnSelectionChanged([this](int boxIndex, int slotIndex) {
        LOG_DEBUG("Bank box selection changed: Box " + std::to_string(boxIndex) + ", Slot " + std::to_string(slotIndex));
    });
    pokemonSaveBox->SetOnSelectionChanged([this](int boxIndex, int slotIndex) {
        LOG_DEBUG("Save box selection changed: Box " + std::to_string(boxIndex) + ", Slot " + std::to_string(slotIndex));
    });

    SetActiveBox(ActiveBox::Save);

    LOG_DEBUG("PokemonBoxes initialization complete");
}

void StorageScreen::LoadBoxData() {
    LOG_DEBUG("Loading box data from provider...");

    // reset Save Box data before loading new box data
    if (pokemonSaveBox) {
        pokemonSaveBox->SetBoxCount(0);
        pokemonSaveBox->SetCurrentBox(0);
    }

    auto currentSave = saveDataAccessor->getCurrentSaveData();

    // Bank boxes
    if (pokemonBankBox && bankBoxDataProvider) {
        const size_t bankCount = bankBoxDataProvider->GetBoxCount(currentSave);
        pokemonBankBox->SetBoxCount(bankCount);
        for (size_t i = 0; i < bankCount; ++i) {
            auto boxData = bankBoxDataProvider->GetBoxData(currentSave, static_cast<int>(i));
            pokemonBankBox->SetBoxData(static_cast<int>(i), boxData);
        }
        pokemonBankBox->SetCurrentBox(0);
    }

    // Save boxes
    if (!currentSave || !pokemonSaveBox || !boxDataProvider) {
        LOG_DEBUG("No save data available, using fallback box data");
        // Set a default box count if no save data available
        if (pokemonSaveBox) {
            pokemonSaveBox->SetBoxCount(1);
            pksm::ui::BoxData emptyBox;
            emptyBox.name = "Box 1";
            pokemonSaveBox->SetBoxData(0, emptyBox);
            // start at box 0
            pokemonSaveBox->SetCurrentBox(0);
        }
        LOG_DEBUG("Fallback box data loaded");
        return;
    }

    const size_t boxCount = boxDataProvider->GetBoxCount(currentSave);
    LOG_DEBUG("Setting box count to " + std::to_string(boxCount));
    pokemonSaveBox->SetBoxCount(boxCount);

    for (size_t i = 0; i < boxCount; ++i) {
        auto boxData = boxDataProvider->GetBoxData(currentSave, static_cast<int>(i));
        pokemonSaveBox->SetBoxData(static_cast<int>(i), boxData);
    }

    pokemonSaveBox->SetCurrentBox(0);
    LOG_DEBUG("Box data loaded successfully");
}

StorageScreen::~StorageScreen() = default;

void StorageScreen::OnInput(u64 down, u64 up, u64 held) {
    if (isSummaryOverlayVisible) {
        if (down & HidNpadButton_B) {
            onHideOverlay();
            isSummaryOverlayVisible = false;
            SetActiveBox(activeBox);
        }
        return;
    }

    if (HandleHelpInput(down)) {
        return;
    }

    static constexpr int ITEMS_PER_ROW = 6;
    bool shouldHandleBoxSwitch = false;

    if (activeBox == ActiveBox::Save && pokemonSaveBox) {
        const int slotIndex = pokemonSaveBox->GetSelectedSlot();
        if (slotIndex >= 0) {
            shouldHandleBoxSwitch = (slotIndex % ITEMS_PER_ROW) == 0;
        }
    } else if (activeBox == ActiveBox::Bank && pokemonBankBox) {
        const int slotIndex = pokemonBankBox->GetSelectedSlot();
        if (slotIndex >= 0) {
            shouldHandleBoxSwitch = (slotIndex % ITEMS_PER_ROW) == (ITEMS_PER_ROW - 1);
        }
    }

    // process directional inputs for cross-box switching at the box edge
    if (shouldHandleBoxSwitch) {
        pokemonBoxDirectionalHandler.HandleInput(down, held);
    }

    // Process button inputs
    buttonHandler.HandleInput(down, up, held);
}

std::vector<pksm::ui::HelpItem> StorageScreen::GetHelpOverlayItems() const {
    return {
        {{{pksm::ui::global::ButtonGlyph::A}}, "Select Pokémon"},
        {{{pksm::ui::global::ButtonGlyph::B}}, "Back to Main Menu"},
        {{{pksm::ui::global::ButtonGlyph::DPad}, {pksm::ui::global::ButtonGlyph::AnalogStick}}, "Navigate Box"},
        {{{pksm::ui::global::ButtonGlyph::L}}, "Previous Box"},
        {{{pksm::ui::global::ButtonGlyph::R}}, "Next Box"},
        {{{pksm::ui::global::ButtonGlyph::Minus}}, "Close Help"}
    };
}

void StorageScreen::OnHelpOverlayShown() {
    LOG_DEBUG("Help overlay shown, disabling UI elements");
    if (pokemonBankBox) {
        pokemonBankBox->SetDisabled(true);
    }
    if (pokemonSaveBox) {
        pokemonSaveBox->SetDisabled(true);
    }
}

void StorageScreen::OnHelpOverlayHidden() {
    LOG_DEBUG("Help overlay hidden, re-enabling UI elements");
    SetActiveBox(activeBox);
}

void StorageScreen::SetActiveBox(ActiveBox box) {
    static constexpr int ITEMS_PER_ROW = 6;

    const ActiveBox previousBox = activeBox;
    int previousSelectedSlot = -1;
    if (previousBox == ActiveBox::Bank && pokemonBankBox) {
        previousSelectedSlot = pokemonBankBox->GetSelectedSlot();
    } else if (previousBox == ActiveBox::Save && pokemonSaveBox) {
        previousSelectedSlot = pokemonSaveBox->GetSelectedSlot();
    }

    activeBox = box;

    if (pokemonBankBox) {
        pokemonBankBox->SetDisabled(activeBox != ActiveBox::Bank);
    }
    if (pokemonSaveBox) {
        pokemonSaveBox->SetDisabled(activeBox != ActiveBox::Save);
    }

    if (activeBox == ActiveBox::Bank && pokemonBankBox) {
        if (previousSelectedSlot >= 0) {
            const int row = previousSelectedSlot / ITEMS_PER_ROW;
            pokemonBankBox->SetSelectedSlot(row * ITEMS_PER_ROW + (ITEMS_PER_ROW - 1));
        }
        pokemonBankBox->RequestFocus();
    } else if (activeBox == ActiveBox::Save && pokemonSaveBox) {
        if (previousSelectedSlot >= 0) {
            const int row = previousSelectedSlot / ITEMS_PER_ROW;
            pokemonSaveBox->SetSelectedSlot(row * ITEMS_PER_ROW);
        }
        pokemonSaveBox->RequestFocus();
    }

    pokemonBoxDirectionalHandler.ClearState();
}

void StorageScreen::PickUp() {
    const bool isBank = (activeBox == ActiveBox::Bank);
    pksm::ui::PokemonBox::Ref targetBox = isBank ? pokemonBankBox : pokemonSaveBox;
    IBoxDataProvider::Ref provider = isBank ? bankBoxDataProvider : boxDataProvider;

    if (!targetBox || !provider) return;

    const int boxIndex = targetBox->GetCurrentBox();
    const int slotIndex = targetBox->GetSelectedSlot();
    if (slotIndex < 0) return;

    const auto slotData = targetBox->GetPokemonData(boxIndex, slotIndex);
    if (slotData.isEmpty()) return;

    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    auto pk = provider->GetPokemon(saveData, boxIndex, slotIndex);
    if (!pk) return;

    // Store the held Pokemon and visually clear the source slot
    heldPokemon = HeldPokemon{std::move(pk), provider, boxIndex, slotIndex, isBank};
    targetBox->SetPokemonData(boxIndex, slotIndex, pksm::ui::BoxPokemonData());

    LOG_DEBUG("Picked up Pokemon from " + std::string(isBank ? "Bank" : "Save") +
              " Box " + std::to_string(boxIndex) + " Slot " + std::to_string(slotIndex));
    UpdateHelpFooter();
}

void StorageScreen::PlaceDown() {
    if (!heldPokemon.has_value()) return;

    const bool destIsBank = (activeBox == ActiveBox::Bank);
    pksm::ui::PokemonBox::Ref destBox = destIsBank ? pokemonBankBox : pokemonSaveBox;
    IBoxDataProvider::Ref destProvider = destIsBank ? bankBoxDataProvider : boxDataProvider;

    if (!destBox || !destProvider) return;

    const int destBoxIndex = destBox->GetCurrentBox();
    const int destSlotIndex = destBox->GetSelectedSlot();
    if (destSlotIndex < 0) return;

    // Placing back on the same slot = cancel
    if (destProvider == heldPokemon->sourceProvider &&
        destBoxIndex == heldPokemon->sourceBox &&
        destSlotIndex == heldPokemon->sourceSlot) {
        CancelPickUp();
        return;
    }

    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    const auto destSlotData = destBox->GetPokemonData(destBoxIndex, destSlotIndex);

    if (destSlotData.isEmpty()) {
        // Place into empty slot
        if (!destProvider->WritePokemon(saveData, destBoxIndex, destSlotIndex, *heldPokemon->pkx)) {
            LOG_ERROR("Failed to write Pokemon to destination");
            CancelPickUp();
            return;
        }
        // Clear the source slot in the provider (data is already visually cleared)
        heldPokemon->sourceProvider->ClearSlot(saveData, heldPokemon->sourceBox, heldPokemon->sourceSlot);

        // Update destination visual
        const auto& pk = *heldPokemon->pkx;
        const u16 form_u16 = pk.alternativeForm();
        const u8 form = form_u16 > 255 ? 0 : static_cast<u8>(form_u16);
        destBox->SetPokemonData(destBoxIndex, destSlotIndex,
            pksm::ui::BoxPokemonData(static_cast<u16>(pk.species()), form, pk.shiny()));

        LOG_DEBUG("Placed Pokemon into " + std::string(destIsBank ? "Bank" : "Save") +
                  " Box " + std::to_string(destBoxIndex) + " Slot " + std::to_string(destSlotIndex));
    } else {
        // Swap: read destination Pokemon, write held to destination, write dest to source
        auto destPk = destProvider->GetPokemon(saveData, destBoxIndex, destSlotIndex);
        if (!destPk) {
            LOG_ERROR("Failed to read destination Pokemon for swap");
            CancelPickUp();
            return;
        }

        if (!destProvider->WritePokemon(saveData, destBoxIndex, destSlotIndex, *heldPokemon->pkx)) {
            LOG_ERROR("Failed to write held Pokemon to destination during swap");
            CancelPickUp();
            return;
        }

        if (!heldPokemon->sourceProvider->WritePokemon(saveData, heldPokemon->sourceBox, heldPokemon->sourceSlot, *destPk)) {
            LOG_ERROR("Failed to write destination Pokemon to source during swap");
            // Destination already written — try to restore it
            destProvider->WritePokemon(saveData, destBoxIndex, destSlotIndex, *destPk);
            CancelPickUp();
            return;
        }

        // Update both slot visuals
        const auto& heldPkRef = *heldPokemon->pkx;
        const u16 heldForm = heldPkRef.alternativeForm();
        destBox->SetPokemonData(destBoxIndex, destSlotIndex,
            pksm::ui::BoxPokemonData(static_cast<u16>(heldPkRef.species()),
                                     heldForm > 255 ? 0 : static_cast<u8>(heldForm),
                                     heldPkRef.shiny()));

        const u16 destForm = destPk->alternativeForm();
        // Update source slot visual — find the right PokemonBox for the source side
        pksm::ui::PokemonBox::Ref sourceBox = heldPokemon->fromBank ? pokemonBankBox : pokemonSaveBox;
        if (sourceBox) {
            sourceBox->SetPokemonData(heldPokemon->sourceBox, heldPokemon->sourceSlot,
                pksm::ui::BoxPokemonData(static_cast<u16>(destPk->species()),
                                         destForm > 255 ? 0 : static_cast<u8>(destForm),
                                         destPk->shiny()));
        }

        LOG_DEBUG("Swapped Pokemon between " + std::string(heldPokemon->fromBank ? "Bank" : "Save") +
                  " and " + std::string(destIsBank ? "Bank" : "Save"));
    }

    heldPokemon.reset();
    UpdateHelpFooter();
}

void StorageScreen::CancelPickUp() {
    if (!heldPokemon.has_value()) return;

    // Restore the source slot visual from the held PKX data
    pksm::ui::PokemonBox::Ref sourceBox = heldPokemon->fromBank ? pokemonBankBox : pokemonSaveBox;
    if (sourceBox) {
        const auto& pk = *heldPokemon->pkx;
        const u16 form_u16 = pk.alternativeForm();
        const u8 form = form_u16 > 255 ? 0 : static_cast<u8>(form_u16);
        sourceBox->SetPokemonData(heldPokemon->sourceBox, heldPokemon->sourceSlot,
            pksm::ui::BoxPokemonData(static_cast<u16>(pk.species()), form, pk.shiny()));
    }

    LOG_DEBUG("Cancelled pick-up");
    heldPokemon.reset();
    UpdateHelpFooter();
}

void StorageScreen::UpdateHelpFooter() {
    if (!helpFooter) return;

    std::vector<pksm::ui::HelpItem> helpItems;
    if (heldPokemon.has_value()) {
        helpItems = {
            {{{pksm::ui::global::ButtonGlyph::A}}, "Place / Swap"},
            {{{pksm::ui::global::ButtonGlyph::B}}, "Cancel"},
            {{{pksm::ui::global::ButtonGlyph::L}, {pksm::ui::global::ButtonGlyph::R}}, "Switch Box"},
            {{{pksm::ui::global::ButtonGlyph::DPad}}, "Navigate Box"},
        };
    } else {
        helpItems = {
            {{{pksm::ui::global::ButtonGlyph::A}}, "Pick Up"},
            {{{pksm::ui::global::ButtonGlyph::B}}, "Back to Main Menu"},
            {{{pksm::ui::global::ButtonGlyph::X}}, "Summary"},
            {{{pksm::ui::global::ButtonGlyph::L}, {pksm::ui::global::ButtonGlyph::R}}, "Switch Box"},
            {{{pksm::ui::global::ButtonGlyph::DPad}}, "Navigate Box"},
        };
    }
    helpFooter->SetHelpItems(helpItems);
}

}  // namespace pksm::layout