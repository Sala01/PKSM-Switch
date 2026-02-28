#pragma once

#include <string>

#include <pu/ui/ui_Overlay.hpp>

#include <pu/ui/elm/elm_Rectangle.hpp>
#include <pu/ui/elm/elm_TextBlock.hpp>

namespace pksm::ui
{

    class ConfirmExitOverlay : public pu::ui::Overlay
    {
    public:
        ConfirmExitOverlay(const pu::i32 x, const pu::i32 y, const pu::i32 width, const pu::i32 height);
        PU_SMART_CTOR(ConfirmExitOverlay)

        void SetSelectedIndex(const int idx);
        void SetTitleText(const std::string &text);

    private:
        void Rebuild();

        int selectedIndex;

        pu::ui::elm::Rectangle::Ref panelShadow;
        pu::ui::elm::Rectangle::Ref panel;
        pu::ui::elm::TextBlock::Ref titleText;

        pu::ui::elm::Rectangle::Ref optionHighlight;
        pu::ui::elm::TextBlock::Ref saveText;
        pu::ui::elm::TextBlock::Ref discardText;
        pu::ui::elm::TextBlock::Ref cancelText;

        static constexpr pu::ui::Color OVERLAY_BG = pu::ui::Color(0, 0, 0, 160);

        static constexpr pu::i32 PANEL_W = 780;
        static constexpr pu::i32 PANEL_H = 420;
        static constexpr pu::i32 PANEL_RADIUS = 28;

        static constexpr pu::ui::Color PANEL_SHADOW = pu::ui::Color(0, 0, 0, 110);
        static constexpr pu::ui::Color PANEL_BG = pu::ui::Color(22, 40, 82, 235);

        static constexpr pu::ui::Color TEXT_DARK = pu::ui::Color(235, 244, 255, 255);
        static constexpr pu::ui::Color OPTION_TEXT = pu::ui::Color(235, 244, 255, 255);
        static constexpr pu::ui::Color OPTION_HL = pu::ui::Color(62, 132, 255, 90);
    };

} // namespace pksm::ui