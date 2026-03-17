#pragma once

#include <memory>
#include <functional>
#include <pu/Plutonium>
#include <atomic>
#include <string>

#include "gui/shared/components/BaseLayout.hpp"
#include "gui/shared/components/FocusableImage.hpp"

namespace pksm::layout
{

    class StartupScreen : public BaseLayout
    {
    private:
        static constexpr u32 LOGO_SIZE = 256;
        static constexpr u32 LOGO_Y = 120;
        static constexpr u32 TEXT_Y = LOGO_Y + LOGO_SIZE + 60;
        static constexpr u32 LOADING_TEXT_Y = TEXT_Y + 80;
        static constexpr u32 LOADING_BAR_Y = LOADING_TEXT_Y + 80;
        static constexpr u32 LOADING_BAR_WIDTH = 600;
        static constexpr u32 LOADING_BAR_HEIGHT = 8;

        // UI Elements
        pksm::ui::FocusableImage::Ref logoImage;
        pu::ui::elm::TextBlock::Ref titleText;
        pu::ui::elm::TextBlock::Ref loadingText;
        pu::ui::elm::Rectangle::Ref loadingBarBackground;
        pu::ui::elm::Rectangle::Ref loadingBarFill;

        std::function<void()> onTimeout;
        std::function<void()> onSkip;
        std::atomic<bool> completed;

    public:
        StartupScreen(
            std::function<void()> onTimeout,
            std::function<void()> onSkip = nullptr,
            std::function<void(pu::ui::Overlay::Ref)> onShowOverlay = nullptr,
            std::function<void()> onHideOverlay = nullptr);
        PU_SMART_CTOR(StartupScreen)

        // Destructor to clean up thread
        ~StartupScreen();

        // BaseLayout overrides
        void OnHelpOverlayShown() override;
        void OnHelpOverlayHidden() override;

        // Input handling method (not override)
        void OnInput(u64 down, u64 up, u64 held);

        // Progress screen controls
        void SetProgress(float normalizedProgress);
        void SetLoadingText(const std::string &text);
        void Complete();
    };

} // namespace pksm::layout