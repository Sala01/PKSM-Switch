#include "gui/screens/settings-screen/SettingsScreen.hpp"
#include "gui/shared/UIConstants.hpp"
#include "utils/Logger.hpp"
#include "utils/SettingsManager.hpp"
#include "utils/NotificationManager.hpp"
#include <iomanip>
#include <sstream>

namespace pksm::layout {

SettingsScreen::SettingsScreen(
    std::function<void()> onBack,
    std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
    std::function<void()> onHideOverlay,
    std::function<void()> onShowEmulatorConfig
)
  : BaseLayout(onShowOverlay, onHideOverlay),
    onBack(onBack),
    onShowEmulatorConfig(onShowEmulatorConfig),
    buttonHandler(),
    settingsManager(nullptr) {
    
    LOG_DEBUG("Initializing SettingsScreen...");

    // Initialize settings manager and store reference
    settingsManager = &pksm::utils::SettingsManager::getInstance();
    if (!settingsManager->initialize()) {
        LOG_WARNING("Failed to initialize settings manager, using defaults");
    }

    this->SetBackgroundColor(bgColor);
    background = ui::AnimatedBackground::New();
    this->Add(background);

    // Create header text
    headerText = pu::ui::elm::TextBlock::New(SIDE_MARGIN, HEADER_TOP_MARGIN, "Settings");
    headerText->SetColor(pksm::ui::global::TEXT_WHITE);
    headerText->SetFont(pksm::ui::global::MakeHeavyFontName(pksm::ui::global::FONT_SIZE_TITLE));
    this->Add(headerText);

    pu::i32 currentY = CONTENT_TOP_MARGIN;

    // General Section
    CreateSectionHeader("General", currentY);
    currentY += SECTION_SPACING;

    CreateSettingButton(
        "Language",
        settingsManager->getString("language", "English"),
        currentY,
        [this]() { 
            LOG_DEBUG("Language setting clicked");
            pksm::utils::NotificationManager::Push("Settings", "Language selection coming soon (i18n needed)");
        }
    );
    currentY += BUTTON_HEIGHT + BUTTON_SPACING;

    CreateSettingButton(
        "Backup Save",
        settingsManager->getBool("backup_save", true) ? "Enabled" : "Disabled",
        currentY,
        [this]() { 
            LOG_DEBUG("Backup Save setting clicked");
            bool current = settingsManager->getBool("backup_save", true);
            settingsManager->setBool("backup_save", !current);
            settingsManager->save();
            UpdateSettingButtonText("Backup Save", !current ? "Enabled" : "Disabled");
            pksm::utils::NotificationManager::Push("Settings", std::string("Backup Save on Load ") + (!current ? "enabled." : "disabled."));
        }
    );
    currentY += BUTTON_HEIGHT + BUTTON_SPACING;

    // Advanced Section
    CreateSectionHeader("Advanced", currentY);
    currentY += SECTION_SPACING;

    CreateSettingButton(
        "Debug Mode",
        settingsManager->getBool("debug_mode", false) ? "Enabled" : "Disabled",
        currentY,
        [this]() { 
            LOG_DEBUG("Debug mode setting clicked");
            bool current = settingsManager->getBool("debug_mode", false);
            settingsManager->setBool("debug_mode", !current);
            settingsManager->save();
            UpdateSettingButtonText("Debug Mode", !current ? "Enabled" : "Disabled");
            pksm::utils::NotificationManager::Push("Settings", 
                std::string("Debug mode ") + (!current ? "enabled" : "disabled"));
        }
    );
    currentY += BUTTON_HEIGHT + BUTTON_SPACING;

    size_t cacheSize = settingsManager->getCacheSize();
    std::string cacheSizeText;
    if (cacheSize == 0) {
        cacheSizeText = "Empty";
    } else {
        std::ostringstream oss;
        if (cacheSize < 1024) {
            oss << cacheSize << " B";
        } else if (cacheSize < 1024 * 1024) {
            oss << std::fixed << std::setprecision(1) << (cacheSize / 1024.0) << " KB";
        } else {
            oss << std::fixed << std::setprecision(1) << (cacheSize / (1024.0 * 1024.0)) << " MB";
        }
        cacheSizeText = oss.str();
    }

    CreateSettingButton(
        "Clear Cache",
        cacheSizeText,
        currentY,
        [this]() { 
            LOG_DEBUG("Clear cache setting clicked");
            if (settingsManager->clearCache()) {
                RefreshCacheSize();
                pksm::utils::NotificationManager::Push("Settings", "Cache cleared successfully.");
            } else {
                pksm::utils::NotificationManager::Push("Settings", "Failed to clear cache.");
            }
        }
    );
    currentY += BUTTON_HEIGHT + BUTTON_SPACING;

    // Reconfigure Section
    CreateSectionHeader("Reconfigure", currentY);
    currentY += SECTION_SPACING;

    CreateSettingButton(
        "Reconfigure Emulator Saves",
        "",
        currentY,
        [this, onShowEmulatorConfig]() { 
            LOG_DEBUG("Reconfigure emulator saves setting clicked");
            if (onShowEmulatorConfig) {
                onShowEmulatorConfig();
            } else {
                pksm::utils::NotificationManager::Push("Settings", "Emulator configuration not available.");
            }
        }
    );
    currentY += BUTTON_HEIGHT + BUTTON_SPACING;

    // Initialize focus manager
    focusManager = pksm::input::FocusManager::New("SettingsScreen Manager");
    focusManager->SetActive(true);
    for (auto& button : settingButtons) {
        focusManager->RegisterFocusable(button);
    }

    InitializeHelpFooter();

    // Set help items
    std::vector<pksm::ui::HelpItem> helpItems = {
        {{{pksm::ui::global::ButtonGlyph::A}}, "Select Setting"},
        {{{pksm::ui::global::ButtonGlyph::B}}, "Back"},
        {{{pksm::ui::global::ButtonGlyph::Minus}}, "Help"}
    };
    helpFooter->SetHelpItems(helpItems);

    // Setup back button handler
    buttonHandler.RegisterButton(
        HidNpadButton_B,
        nullptr,
        [this]() {
            LOG_DEBUG("B button pressed, returning to previous screen");
            if (this->onBack) {
                this->onBack();
            }
        }
    );

    // Set up input handling
    this->SetOnInput(
        std::bind(&SettingsScreen::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );

    LOG_DEBUG("SettingsScreen initialization complete");
}

SettingsScreen::~SettingsScreen() = default;

void SettingsScreen::CreateSectionHeader(const std::string& text, pu::i32 y) {
    auto header = pu::ui::elm::TextBlock::New(SIDE_MARGIN, y, text);
    header->SetColor(pksm::ui::global::TEXT_WHITE);
    header->SetFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_HEADER));
    this->Add(header);
}

void SettingsScreen::CreateSettingButton(
    const std::string& label,
    const std::string& value,
    pu::i32 y,
    std::function<void()> onClick
) {
    auto button = pksm::ui::FocusableButton::New(
        SIDE_MARGIN,
        y,
        BUTTON_WIDTH,
        BUTTON_HEIGHT,
        label + ": " + value,
        pu::ui::Color(40, 40, 60, 200),
        pu::ui::Color(60, 60, 100, 255)
    );
    
    button->SetContentFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_BUTTON));
    button->SetContentColor(pksm::ui::global::TEXT_WHITE);
    button->SetOnClick(onClick);
    button->SetHelpText("Change " + label);
    
    this->Add(button);
    settingButtons.push_back(button);
}

void SettingsScreen::OnInput(u64 down, u64 up, u64 held) {
    if (HandleHelpInput(down)) {
        return;  // Input was handled by help system
    }

    buttonHandler.HandleInput(down, up, held);

    // Handle focus navigation with D-pad
    if (down & HidNpadButton_Down) {
        auto currentFocused = focusManager->GetFocusedElement();
        if (currentFocused) {
            for (size_t i = 0; i < settingButtons.size(); i++) {
                if (settingButtons[i] == currentFocused && i + 1 < settingButtons.size()) {
                    settingButtons[i + 1]->RequestFocus();
                    break;
                }
            }
        } else if (!settingButtons.empty()) {
            settingButtons[0]->RequestFocus();
        }
    } else if (down & HidNpadButton_Up) {
        // Move to previous button
        auto currentFocused = focusManager->GetFocusedElement();
        if (currentFocused) {
            for (size_t i = 0; i < settingButtons.size(); i++) {
                if (settingButtons[i] == currentFocused && i > 0) {
                    settingButtons[i - 1]->RequestFocus();
                    break;
                }
            }
        } else if (!settingButtons.empty()) {
            settingButtons[0]->RequestFocus();
        }
    }

    // Let focused button handle A button press - call OnInput directly on the button
    auto focused = focusManager->GetFocusedElement();
    if (focused && (down & HidNpadButton_A)) {
        for (auto& button : settingButtons) {
            if (button == focused) {
                button->OnInput(down, up, held, pu::ui::TouchPoint());
                break;
            }
        }
    }
}

std::vector<pksm::ui::HelpItem> SettingsScreen::GetHelpOverlayItems() const {
    return {
        {{{pksm::ui::global::ButtonGlyph::A}}, "Select Setting"},
        {{{pksm::ui::global::ButtonGlyph::B}}, "Back"},
        {{{pksm::ui::global::ButtonGlyph::Minus}}, "Help"},
        {{{pksm::ui::global::ButtonGlyph::DPad}}, "Navigate"}
    };
}

void SettingsScreen::OnHelpOverlayShown() {
    for (auto& button : settingButtons) {
        button->SetVisible(false);
    }
    headerText->SetVisible(false);
}

void SettingsScreen::OnHelpOverlayHidden() {
    for (auto& button : settingButtons) {
        button->SetVisible(true);
    }
    headerText->SetVisible(true);
}

void SettingsScreen::UpdateSettingButtonText(const std::string& label, const std::string& newValue) {
    for (auto& button : settingButtons) {
        std::string currentText = button->GetContent();
        size_t colonPos = currentText.find(": ");
        if (colonPos != std::string::npos) {
            std::string currentLabel = currentText.substr(0, colonPos);
            if (currentLabel == label) {
                button->SetContent(label + ": " + newValue);
                break;
            }
        }
    }
}

void SettingsScreen::RefreshCacheSize() {
    size_t cacheSize = settingsManager->getCacheSize();
    std::string cacheSizeText;
    if (cacheSize == 0) {
        cacheSizeText = "Empty";
    } else {
        std::ostringstream oss;
        if (cacheSize < 1024) {
            oss << cacheSize << " B";
        } else if (cacheSize < 1024 * 1024) {
            oss << std::fixed << std::setprecision(1) << (cacheSize / 1024.0) << " KB";
        } else {
            oss << std::fixed << std::setprecision(1) << (cacheSize / (1024.0 * 1024.0)) << " MB";
        }
        cacheSizeText = oss.str();
    }
    UpdateSettingButtonText("Clear Cache", cacheSizeText);
}

}  // namespace pksm::layout