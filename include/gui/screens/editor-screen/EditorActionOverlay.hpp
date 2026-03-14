#pragma once

#include <functional>
#include <string>
#include <vector>

#include <pu/ui/ui_Overlay.hpp>
#include <pu/ui/elm/elm_Rectangle.hpp>
#include <pu/ui/elm/elm_TextBlock.hpp>

namespace pksm::ui {

/**
 * A clean action-menu overlay for the Editor screen.
 *
 * Shows a centred modal panel with up to 4 labelled actions.
 * The caller:
 *  1. Constructs it once (or reuses an instance).
 *  2. Calls SetActions() / SetTitle() when opening.
 *  3. Polls GetSelectedIndex() / IsConfirmed() / IsCancelled() from OnInput.
 *
 * Input is handled externally (EditorScreen::OnInput) so that the layout can
 * decide what to do with each action.
 */
class EditorActionOverlay : public pu::ui::Overlay {
public:
    EditorActionOverlay(pu::i32 x, pu::i32 y, pu::i32 width, pu::i32 height);
    PU_SMART_CTOR(EditorActionOverlay)

    // Configure before showing
    void SetTitle(const std::string& title);
    void SetActions(const std::vector<std::string>& actions);

    // Navigation (called from EditorScreen::OnInput while overlay is visible)
    void MoveUp();
    void MoveDown();

    int GetSelectedIndex() const { return selectedIndex; }
    const std::string& GetSelectedAction() const;

    // Reset selection to 0 (call before re-opening)
    void Reset() { SetSelectedIndex(0); }

private:
    void Rebuild();
    void SetSelectedIndex(int idx);

    std::string titleStr;
    std::vector<std::string> actions;
    int selectedIndex = 0;

    // Visual elements (rebuilt when actions change)
    pu::ui::elm::Rectangle::Ref panelShadow;
    pu::ui::elm::Rectangle::Ref panel;
    pu::ui::elm::TextBlock::Ref titleText;
    pu::ui::elm::Rectangle::Ref optionHighlight;
    std::vector<pu::ui::elm::TextBlock::Ref> optionTexts;

    // Layout
    static constexpr pu::i32 PANEL_W      = 620;
    static constexpr pu::i32 OPT_H        = 56;
    static constexpr pu::i32 OPT_GAP      = 10;
    static constexpr pu::i32 HDR_H        = 80;  // header height inside panel
    static constexpr pu::i32 PANEL_PAD_X  = 50;  // left/right padding inside panel
    static constexpr pu::i32 PANEL_RADIUS = 24;

    // Colours
    static constexpr pu::ui::Color OVERLAY_BG   = pu::ui::Color(  0,   0,   0, 170);
    static constexpr pu::ui::Color SHADOW_COL    = pu::ui::Color(  0,   0,   0, 110);
    static constexpr pu::ui::Color PANEL_BG      = pu::ui::Color( 18,  34,  82, 240);
    static constexpr pu::ui::Color TITLE_COL     = pu::ui::Color(200, 220, 255, 255);
    static constexpr pu::ui::Color OPT_TEXT_COL  = pu::ui::Color(235, 245, 255, 255);
    static constexpr pu::ui::Color HL_COL        = pu::ui::Color( 62, 132, 255,  90);
};

}  // namespace pksm::ui
