#include "gui/screens/main-menu/MainMenu.hpp"

#include "gui/screens/main-menu/sub-components/menu-grid/MenuButtonGrid.hpp"
#include "utils/Logger.hpp"
#include "utils/NotificationManager.hpp"

namespace pksm::layout {

MainMenu::MainMenu(
    std::function<void()> onBack,
    std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
    std::function<void()> onHideOverlay,
    ISaveDataAccessor::Ref saveDataAccessor,
    IBoxDataProvider::Ref boxDataProvider,
    IPartyDataProvider::Ref partyDataProvider,
    IBoxDataProvider::Ref bankBoxDataProvider,
    std::map<pksm::ui::MenuButtonType, std::function<void()>> navigationCallbacks
)
  : BaseLayout(onShowOverlay, onHideOverlay),
    onBack(onBack),
    saveDataAccessor(saveDataAccessor),
    boxDataProvider(boxDataProvider),
    partyDataProvider(partyDataProvider),
    bankBoxDataProvider(bankBoxDataProvider),
    navigationCallbacks(navigationCallbacks),
    buttonHandler(),
    isExitConfirmVisible(false),
    exitConfirmSelectedIndex(0),
    exitConfirmOverlay(nullptr),
    exitConfirmPending(false) {
    LOG_DEBUG("Initializing MainMenu...");

    this->SetBackgroundColor(bgColor);
    background = ui::AnimatedBackground::New();
    this->Add(background);

    // Create trainer info component with dynamic height
    trainerInfo = pksm::ui::TrainerInfo::New(
        TRAINER_INFO_SIDE_MARGIN,
        TRAINER_INFO_TOP_MARGIN,
        TRAINER_INFO_WIDTH,
        "",  // OT Name (will be updated)
        0,  // Badges (will be updated)
        0,  // Wonder Cards (will be updated)
        0,  // National Dex Seen (will be updated)
        0,  // National Dex Caught (will be updated)
        0.0f,  // Completion (will be updated)
        ""  // Time played (will be updated)
    );
    this->Add(trainerInfo);

    // Calculate MenuButtonGrid width and position
    const pu::i32 MENU_GRID_MARGIN = 32;  // Margin between TrainerInfo and MenuButtonGrid
    const pu::i32 menuGridWidth = GetWidth() - (TRAINER_INFO_SIDE_MARGIN + TRAINER_INFO_WIDTH + (MENU_GRID_MARGIN * 2));
    const pu::i32 menuGridX = TRAINER_INFO_SIDE_MARGIN + TRAINER_INFO_WIDTH + MENU_GRID_MARGIN;

    // Create menu button grid
    menuGrid = pksm::ui::MenuButtonGrid::New(menuGridX, MENU_GRID_TOP_MARGIN, menuGridWidth);
    this->Add(menuGrid);

    const std::string versionString = std::string("PKSM Switch v") + PKSM_VERSION;
    
    // Load the textbox background image
    pu::sdl2::Texture versionimageTexture = pu::ui::render::LoadImage("romfs:/gfx/ui/textbox_blue.png");
    auto versionimageHandle = pu::sdl2::TextureHandle::New(versionimageTexture);
    
    const pu::i32 VERSION_IMAGE_WIDTH = 750;
    const pu::i32 VERSION_IMAGE_HEIGHT = 85;
    
    versionBackground = pu::ui::elm::Image::New(
        GetWidth() - VERSION_IMAGE_WIDTH - VERSION_IMAGE_RIGHT_MARGIN,
        VERSION_IMAGE_TOP_MARGIN,
        versionimageHandle
    );
    
    versionBackground->SetWidth(VERSION_IMAGE_WIDTH);
    versionBackground->SetHeight(VERSION_IMAGE_HEIGHT);
    this->Add(versionBackground);
    
    versionText = pu::ui::elm::TextBlock::New(
        GetWidth() - VERSION_IMAGE_WIDTH + VERSION_IMAGE_PADDING + 300,
        VERSION_IMAGE_TOP_MARGIN + (VERSION_IMAGE_HEIGHT / 2) - 14,
        versionString
    );

    versionText->SetColor(pu::ui::Color(255, 255, 255, 255));
    versionText->SetFont(pksm::ui::global::MakeHeavyFontName(35));
    this->Add(versionText);

    // Register navigation callbacks for the menu buttons
    RegisterNavigationCallbacks();

    // Initialize help footer
    InitializeHelpFooter();

    // Set initial help items
    std::vector<pksm::ui::HelpItem> helpItems = {
        {{{pksm::ui::global::ButtonGlyph::A}}, "Select Highlighted"},
        {{{pksm::ui::global::ButtonGlyph::B}}, "Back to Game Selection"},
        {{{pksm::ui::global::ButtonGlyph::Y}}, "Save"},
    };
    helpFooter->SetHelpItems(helpItems);

    // Setup button handler
    buttonHandler.RegisterButton(
        HidNpadButton_B,
        nullptr,  // No visual feedback needed
        [this]() {
            const bool saveDirty = (this->saveDataAccessor && this->saveDataAccessor->hasUnsavedChanges());
            const bool boxDirty = (this->boxDataProvider && this->boxDataProvider->HasPendingWrites());
            const bool partyDirty = (this->partyDataProvider && this->partyDataProvider->HasPendingWrites());
            const bool bankDirty = (this->bankBoxDataProvider && this->bankBoxDataProvider->HasPendingWrites());
            if (saveDirty || boxDirty || partyDirty || bankDirty) {
                if (!this->isExitConfirmVisible) {
                    this->exitConfirmSelectedIndex = 0;
                    this->exitConfirmOverlay = pksm::ui::ConfirmExitOverlay::New(0, 0, this->GetWidth(), this->GetHeight());
                    this->exitConfirmOverlay->SetSelectedIndex(this->exitConfirmSelectedIndex);
                    this->onShowOverlay(this->exitConfirmOverlay);
                    this->isExitConfirmVisible = true;
                }
                return;
            }

            LOG_DEBUG("B button pressed, returning to game selection");
            if (this->onBack) {
                this->onBack();
            }
        }
    );

    buttonHandler.RegisterButton(
        HidNpadButton_Y,
        nullptr,
        [this]() {
            const bool bankDirty = (this->bankBoxDataProvider && this->bankBoxDataProvider->HasPendingWrites());
            const bool boxDirty = (this->boxDataProvider && this->boxDataProvider->HasPendingWrites());
            const bool partyDirty = (this->partyDataProvider && this->partyDataProvider->HasPendingWrites());
            const bool saveDirty = (this->saveDataAccessor && this->saveDataAccessor->hasUnsavedChanges());

            if (!saveDirty && !boxDirty && !partyDirty && !bankDirty) {
                pksm::utils::NotificationManager::Push("Save", "No changes to save.");
                return;
            }

            if (saveDirty) {
                if (!this->saveDataAccessor) {
                    pksm::utils::NotificationManager::Push("Save Failed", "No save accessor available.");
                    return;
                }

                if (!this->saveDataAccessor->getCurrentSaveData()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "No save loaded.");
                    return;
                }

                if (!this->saveDataAccessor->saveChanges()) {
                    const auto err = this->saveDataAccessor->getLastError();
                    if (!err.empty()) {
                        pksm::utils::NotificationManager::Push("Save Failed", err);
                    } else {
                        pksm::utils::NotificationManager::Push("Save Failed", "Failed to save changes.");
                    }
                    return;
                }
            }

            if (boxDirty) {
                if (!this->boxDataProvider->FlushPendingWrites()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "Failed to save box changes.");
                    return;
                }
            }

            if (partyDirty) {
                if (!this->partyDataProvider->FlushPendingWrites()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "Failed to save party changes.");
                    return;
                }
            }

            if (bankDirty) {
                if (!this->bankBoxDataProvider->FlushPendingWrites()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "Failed to save bank changes.");
                    return;
                }
            }

            pksm::utils::NotificationManager::Push("Save", "Changes saved successfully.");
        }
    );

    // Set up input handling
    this->SetOnInput(
        std::bind(&MainMenu::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );

    LOG_DEBUG("MainMenu initialization complete");
}

void MainMenu::RegisterNavigationCallbacks() {
    LOG_DEBUG("Registering navigation callbacks for menu buttons");

    // Register each callback with the corresponding button type
    for (const auto& [buttonType, callback] : navigationCallbacks) {
        menuGrid->RegisterButtonCallback(buttonType, callback);
        LOG_DEBUG("Registered callback for button type: " + std::to_string(static_cast<int>(buttonType)));
    }
}

void MainMenu::UpdateTrainerInfo() {
    auto saveData = saveDataAccessor->getCurrentSaveData();
    if (saveData) {
        menuGrid->SetSelectedIndex(0);
        LOG_DEBUG("Updating trainer info with save data");

        // Update trainer info with save data
        trainerInfo->SetTrainerInfo(
            saveData->getOTName(),
            saveData->getBadges(),
            saveData->getWonderCards(),
            saveData->getDexSeen(),
            saveData->getDexCaught(),
            saveData->getDexCompletionPercentage() /
                100.0f,  // Convert from percentage (0-100) to 0-1 range for progress bar
            saveData->getPlayedTimeString(),
            saveData->getGeneration()
        );
    } else {
        LOG_DEBUG("No save data available, using default trainer info");

        // Set default values
        trainerInfo->SetTrainerInfo("No Save Loaded", 0, 0, 0, 0, 0.0f, "00:00:00", pksm::saves::Generation::TWO);
    }
}

MainMenu::~MainMenu() = default;

void MainMenu::OnInput(u64 down, u64 up, u64 held) {
    if (isExitConfirmVisible) {
        if (exitConfirmPending) {
            exitConfirmPending = false;

            if (this->saveDataAccessor && this->saveDataAccessor->hasUnsavedChanges()) {
                if (!this->saveDataAccessor->getCurrentSaveData()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "No save loaded.");
                    return;
                }

                if (!this->saveDataAccessor->saveChanges()) {
                    const auto err = this->saveDataAccessor->getLastError();
                    if (!err.empty()) {
                        pksm::utils::NotificationManager::Push("Save Failed", err);
                    } else {
                        pksm::utils::NotificationManager::Push("Save Failed", "Failed to save changes.");
                    }
                    return;
                }
            }

            if (this->boxDataProvider && this->boxDataProvider->HasPendingWrites()) {
                if (!this->boxDataProvider->FlushPendingWrites()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "Failed to save box changes.");
                    return;
                }
            }

            if (this->partyDataProvider && this->partyDataProvider->HasPendingWrites()) {
                if (!this->partyDataProvider->FlushPendingWrites()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "Failed to save party changes.");
                    return;
                }
            }

            if (this->bankBoxDataProvider && this->bankBoxDataProvider->HasPendingWrites()) {
                if (!this->bankBoxDataProvider->FlushPendingWrites()) {
                    pksm::utils::NotificationManager::Push("Save Failed", "Failed to save bank changes.");
                    return;
                }
            }

            this->onHideOverlay();
            this->isExitConfirmVisible = false;
            this->exitConfirmOverlay = nullptr;
            if (this->onBack) {
                this->onBack();
            }
            return;
        }

        if (down & HidNpadButton_B) {
            this->onHideOverlay();
            this->isExitConfirmVisible = false;
            this->exitConfirmOverlay = nullptr;
            return;
        }

        if (down & HidNpadButton_AnyUp) {
            if (exitConfirmSelectedIndex > 0) {
                exitConfirmSelectedIndex--;
                if (exitConfirmOverlay) {
                    exitConfirmOverlay->SetSelectedIndex(exitConfirmSelectedIndex);
                }
            }
            return;
        }

        if (down & HidNpadButton_AnyDown) {
            if (exitConfirmSelectedIndex < 2) {
                exitConfirmSelectedIndex++;
                if (exitConfirmOverlay) {
                    exitConfirmOverlay->SetSelectedIndex(exitConfirmSelectedIndex);
                }
            }
            return;
        }

        if (down & HidNpadButton_A) {
            if (exitConfirmSelectedIndex == 0) {
                if (exitConfirmOverlay) {
                    exitConfirmOverlay->SetTitleText("Saving... Do Not Close App or Power off.");
                }

                exitConfirmPending = true;
                return;
            }

            if (exitConfirmSelectedIndex == 1) {
                if (this->boxDataProvider && this->boxDataProvider->HasPendingWrites()) {
                    this->boxDataProvider->DiscardPendingWrites();
                }
                if (this->partyDataProvider && this->partyDataProvider->HasPendingWrites()) {
                    this->partyDataProvider->DiscardPendingWrites();
                }
                if (this->bankBoxDataProvider && this->bankBoxDataProvider->HasPendingWrites()) {
                    this->bankBoxDataProvider->DiscardPendingWrites();
                }
            }

            if (exitConfirmSelectedIndex != 2) {
                this->onHideOverlay();
                this->isExitConfirmVisible = false;
                this->exitConfirmOverlay = nullptr;
                if (this->onBack) {
                    this->onBack();
                }
            } else {
                this->onHideOverlay();
                this->isExitConfirmVisible = false;
                this->exitConfirmOverlay = nullptr;
            }
            return;
        }

        return;
    }

    // First handle help-related input
    if (HandleHelpInput(down)) {
        return;  // Input was handled by help system
    }

    // Only process button input if not in help overlay
    buttonHandler.HandleInput(down, up, held);
}

std::vector<pksm::ui::HelpItem> MainMenu::GetHelpOverlayItems() const {
    return {
        {{{pksm::ui::global::ButtonGlyph::A}}, "Select Highlighted"},
        {{{pksm::ui::global::ButtonGlyph::B}}, "Back to Game Selection"},
        {{{pksm::ui::global::ButtonGlyph::Y}}, "Save"},
        {{{pksm::ui::global::ButtonGlyph::DPad}}, "Navigate"},
        {{{pksm::ui::global::ButtonGlyph::Minus}}, "Close Help"}
    };
}

void MainMenu::OnHelpOverlayShown() {
    LOG_DEBUG("Help overlay shown, disabling UI elements");
    menuGrid->SetDisabled(true);
    trainerInfo->SetVisible(false);
    versionBackground->SetVisible(false);
    versionText->SetVisible(false);
}

void MainMenu::OnHelpOverlayHidden() {
    LOG_DEBUG("Help overlay hidden, re-enabling UI elements");
    menuGrid->SetDisabled(false);
    trainerInfo->SetVisible(true);
    versionBackground->SetVisible(true);
    versionText->SetVisible(true);
}

}  // namespace pksm::layout