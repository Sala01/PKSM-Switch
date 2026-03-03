#include "gui/screens/storage-screen/StorageScreen.hpp"

#include <ctime>

#include "gui/screens/main-menu/sub-components/menu-grid/MenuButtonGrid.hpp"
#include "utils/Logger.hpp"

namespace {

pksm::ui::BoxPokemonData PkxToVisual(const pksm::PKX& pk) {
    const u16 form_u16 = pk.alternativeForm();
    const u8 form = form_u16 > 255 ? 0 : static_cast<u8>(form_u16);
    return pksm::ui::BoxPokemonData(static_cast<u16>(pk.species()), form, pk.shiny());
}

} // namespace

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

    // Held Pokemon floating sprite (added after boxes so it renders on top)
    heldPokemonImage = pu::ui::elm::Image::New(0, 0, nullptr);
    heldPokemonImage->SetWidth(BOX_ITEM_SIZE + 12);
    heldPokemonImage->SetHeight(BOX_ITEM_SIZE + 12);
    heldPokemonImage->SetVisible(false);
    this->Add(heldPokemonImage);

    // Load box data for both Bank and Save
    LoadBoxData();

    pokemonBankBox->SetOnSelectionChanged([this](int boxIndex, int slotIndex) {
        LOG_DEBUG("Bank box selection changed: Box " + std::to_string(boxIndex) + ", Slot " + std::to_string(slotIndex));
        UpdateHeldPokemonImage();
    });
    pokemonSaveBox->SetOnSelectionChanged([this](int boxIndex, int slotIndex) {
        LOG_DEBUG("Save box selection changed: Box " + std::to_string(boxIndex) + ", Slot " + std::to_string(slotIndex));
        UpdateHeldPokemonImage();
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
    UpdateHeldPokemonImage();
}

void StorageScreen::PickUp() {
    const bool isBank = (activeBox == ActiveBox::Bank);
    pksm::ui::PokemonBox::Ref targetBox = isBank ? pokemonBankBox : pokemonSaveBox;
    IBoxDataProvider::Ref provider = isBank ? bankBoxDataProvider : boxDataProvider;

    if (!targetBox || !provider) return;

    const int boxIndex = targetBox->GetCurrentBox();
    const int slotIndex = targetBox->GetSelectedSlot();
    if (slotIndex < 0) return;

    const auto slotVisual = targetBox->GetPokemonData(boxIndex, slotIndex);
    if (slotVisual.isEmpty()) return;

    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    auto pk = provider->GetPokemon(saveData, boxIndex, slotIndex);
    if (!pk) return;

    // Store held Pokemon with original source info and visually clear the source slot
    heldPokemon = HeldPokemon{
        std::move(pk), provider, boxIndex, slotIndex, isBank, slotVisual
    };
    deferredWrites.clear();
    targetBox->SetPokemonData(boxIndex, slotIndex, pksm::ui::BoxPokemonData());

    LOG_DEBUG("Picked up Pokemon from " + std::string(isBank ? "Bank" : "Save") +
              " Box " + std::to_string(boxIndex) + " Slot " + std::to_string(slotIndex));
    UpdateHeldPokemonImage();
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

    // Placing back on original slot with no pending swaps = simple cancel
    if (deferredWrites.empty() &&
        destProvider == heldPokemon->originalProvider &&
        destBoxIndex == heldPokemon->originalBox &&
        destSlotIndex == heldPokemon->originalSlot) {
        CancelPickUp();
        return;
    }

    const auto destSlotVisual = destBox->GetPokemonData(destBoxIndex, destSlotIndex);

    if (destSlotVisual.isEmpty()) {
        // ── Place into empty slot: commit all deferred writes + final placement ──
        auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;

        // Convert held Pokemon to destination format before writing
        auto prepared = destProvider->PrepareForWrite(saveData, *heldPokemon->pkx);
        if (!prepared) {
            LOG_DEBUG("Cannot place: incompatible format for destination save");
            return; // User stays holding, placement blocked
        }

        bool allOk = true;

        // Flush all deferred writes to disk (already converted at carry-swap time)
        for (const auto& dw : deferredWrites) {
            if (!dw.provider->WritePokemon(saveData, dw.boxIndex, dw.slotIndex, *dw.pkx)) {
                LOG_ERROR("Failed to commit deferred write at Box " +
                          std::to_string(dw.boxIndex) + " Slot " + std::to_string(dw.slotIndex));
                allOk = false;
            }
        }

        // Write converted Pokemon to the final destination
        if (!destProvider->WritePokemon(saveData, destBoxIndex, destSlotIndex, *prepared)) {
            LOG_ERROR("Failed to write held Pokemon to final destination");
            allOk = false;
        }

        // Clear the original source on disk, unless a deferred write or the
        // final destination already covers that slot
        bool originalCovered = (destProvider == heldPokemon->originalProvider &&
                                destBoxIndex == heldPokemon->originalBox &&
                                destSlotIndex == heldPokemon->originalSlot);
        if (!originalCovered) {
            for (const auto& dw : deferredWrites) {
                if (dw.provider == heldPokemon->originalProvider &&
                    dw.boxIndex == heldPokemon->originalBox &&
                    dw.slotIndex == heldPokemon->originalSlot) {
                    originalCovered = true;
                    break;
                }
            }
        }
        if (!originalCovered) {
            heldPokemon->originalProvider->ClearSlot(
                saveData, heldPokemon->originalBox, heldPokemon->originalSlot);
        }

        // Update destination visual
        destBox->SetPokemonData(destBoxIndex, destSlotIndex, PkxToVisual(*prepared));

        if (!allOk) {
            LOG_ERROR("Some writes failed during commit — reload box data for accurate state");
        }
        LOG_DEBUG("Committed " + std::to_string(deferredWrites.size()) +
                  " deferred writes + final placement at " +
                  std::string(destIsBank ? "Bank" : "Save") +
                  " Box " + std::to_string(destBoxIndex) +
                  " Slot " + std::to_string(destSlotIndex));

        deferredWrites.clear();
        heldPokemon.reset();
        UpdateHeldPokemonImage();
        UpdateHelpFooter();

    } else {
        // ── Carry-swap: displace the destination Pokemon and keep carrying ──

        // Check if a deferred write already targets this slot
        DeferredWrite* existing = nullptr;
        for (auto& dw : deferredWrites) {
            if (dw.provider == destProvider &&
                dw.boxIndex == destBoxIndex &&
                dw.slotIndex == destSlotIndex) {
                existing = &dw;
                break;
            }
        }

        if (existing) {
            // Slot already has a deferred write — swap PKX pointers
            auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
            auto prepared = destProvider->PrepareForWrite(saveData, *heldPokemon->pkx);
            if (!prepared) {
                LOG_DEBUG("Cannot swap: incompatible format for destination");
                return;
            }
            auto displaced = std::move(existing->pkx);
            existing->pkx = std::move(prepared);
            heldPokemon->pkx = std::move(displaced);
            // previousVisual stays unchanged (original disk state)
            destBox->SetPokemonData(destBoxIndex, destSlotIndex, PkxToVisual(*existing->pkx));
        } else {
            // New slot — read the current occupant from disk
            auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;

            auto prepared = destProvider->PrepareForWrite(saveData, *heldPokemon->pkx);
            if (!prepared) {
                LOG_DEBUG("Cannot swap: incompatible format for destination");
                return;
            }

            auto displaced = destProvider->GetPokemon(saveData, destBoxIndex, destSlotIndex);
            if (!displaced) {
                LOG_ERROR("Failed to read destination Pokemon for carry-swap");
                return;
            }

            DeferredWrite dw{
                destProvider, destBoxIndex, destSlotIndex, destIsBank,
                std::move(prepared), destSlotVisual
            };
            destBox->SetPokemonData(destBoxIndex, destSlotIndex, PkxToVisual(*dw.pkx));
            deferredWrites.push_back(std::move(dw));
            heldPokemon->pkx = std::move(displaced);
        }

        // Update held sprite (held Pokemon changed after swap)
        UpdateHeldPokemonImage();

        LOG_DEBUG("Carry-swap at " + std::string(destIsBank ? "Bank" : "Save") +
                  " Box " + std::to_string(destBoxIndex) +
                  " Slot " + std::to_string(destSlotIndex) +
                  " (chain length: " + std::to_string(deferredWrites.size()) + ")");
    }
}

void StorageScreen::CancelPickUp() {
    if (!heldPokemon.has_value()) return;

    // Restore all deferred write visuals in reverse order
    for (auto it = deferredWrites.rbegin(); it != deferredWrites.rend(); ++it) {
        pksm::ui::PokemonBox::Ref box = it->isBank ? pokemonBankBox : pokemonSaveBox;
        if (box) {
            box->SetPokemonData(it->boxIndex, it->slotIndex, it->previousVisual);
        }
    }

    // Restore the original source slot visual
    pksm::ui::PokemonBox::Ref sourceBox = heldPokemon->originalFromBank ? pokemonBankBox : pokemonSaveBox;
    if (sourceBox) {
        sourceBox->SetPokemonData(heldPokemon->originalBox, heldPokemon->originalSlot,
                                  heldPokemon->originalVisual);
    }

    LOG_DEBUG("Cancelled pick-up, restored " + std::to_string(deferredWrites.size()) + " deferred writes");
    deferredWrites.clear();
    heldPokemon.reset();
    UpdateHeldPokemonImage();
    UpdateHelpFooter();
}

void StorageScreen::UpdateHeldPokemonImage() {
    if (!heldPokemonImage) return;

    if (!heldPokemon.has_value()) {
        heldPokemonImage->SetVisible(false);
        return;
    }

    // Update sprite to match the currently held Pokemon (may change after carry-swap)
    auto visual = PkxToVisual(*heldPokemon->pkx);
    heldPokemonImage->SetImage(visual.getSprite());
    heldPokemonImage->SetWidth(BOX_ITEM_SIZE + 12);
    heldPokemonImage->SetHeight(BOX_ITEM_SIZE + 12);

    // Position at the currently selected slot in the active box
    auto targetBox = (activeBox == ActiveBox::Bank) ? pokemonBankBox : pokemonSaveBox;
    if (targetBox) {
        auto [sx, sy] = targetBox->GetSelectedSlotScreenPosition();
        // Offset by -6 to match BoxItem's spriteOverscan centering
        heldPokemonImage->SetX(sx - 20);
        heldPokemonImage->SetY(sy - 20);
    }

    heldPokemonImage->SetVisible(true);
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