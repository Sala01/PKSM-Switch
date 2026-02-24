#include "gui/shared/components/ConfirmExitOverlay.hpp"

#include "gui/shared/UIConstants.hpp"

namespace pksm::ui {

ConfirmExitOverlay::ConfirmExitOverlay(const pu::i32 x, const pu::i32 y, const pu::i32 width, const pu::i32 height)
  : pu::ui::Overlay(x, y, width, height, OVERLAY_BG), selectedIndex(0) {
    this->SetRadius(0);
    this->SetMaxFadeAlpha(200);
    this->SetFadeAlphaVariation(18);
    this->Rebuild();
}

void ConfirmExitOverlay::SetSelectedIndex(const int idx) {
    if (idx < 0) {
        selectedIndex = 0;
    } else if (idx > 2) {
        selectedIndex = 2;
    } else {
        selectedIndex = idx;
    }

    if (optionHighlight) {
        const pu::i32 panelX = (this->GetWidth() - PANEL_W) / 2;
        const pu::i32 panelY = (this->GetHeight() - PANEL_H) / 2;

        const pu::i32 optX = panelX + 60;
        const pu::i32 optY0 = panelY + 190;
        const pu::i32 optH = 58;
        const pu::i32 optGap = 14;

        const pu::i32 hlY = optY0 + (selectedIndex * (optH + optGap));
        optionHighlight->SetX(optX);
        optionHighlight->SetY(hlY);
    }
}

void ConfirmExitOverlay::Rebuild() {
    this->Clear();

    const pu::i32 panelX = (this->GetWidth() - PANEL_W) / 2;
    const pu::i32 panelY = (this->GetHeight() - PANEL_H) / 2;

    panelShadow = pu::ui::elm::Rectangle::New(panelX + 10, panelY + 10, PANEL_W, PANEL_H, PANEL_SHADOW);
    panelShadow->SetBorderRadius(PANEL_RADIUS);
    this->Add(panelShadow);

    panel = pu::ui::elm::Rectangle::New(panelX, panelY, PANEL_W, PANEL_H, PANEL_BG);
    panel->SetBorderRadius(PANEL_RADIUS);
    this->Add(panel);

    titleText = pu::ui::elm::TextBlock::New(panelX + 40, panelY + 38, "Save changes before exiting?");
    titleText->SetFont(pksm::ui::global::MakeHeavyFontName(36));
    titleText->SetColor(TEXT_DARK);
    this->Add(titleText);

    const pu::i32 optX = panelX + 60;
    const pu::i32 optY0 = panelY + 190;
    const pu::i32 optW = PANEL_W - 120;
    const pu::i32 optH = 58;
    const pu::i32 optGap = 14;

    optionHighlight = pu::ui::elm::Rectangle::New(optX, optY0, optW, optH, OPTION_HL);
    optionHighlight->SetBorderRadius(18);
    this->Add(optionHighlight);

    saveText = pu::ui::elm::TextBlock::New(optX + 20, optY0 + 12, "Save and Exit");
    saveText->SetFont(pksm::ui::global::MakeMediumFontName(28));
    saveText->SetColor(OPTION_TEXT);
    this->Add(saveText);

    discardText = pu::ui::elm::TextBlock::New(optX + 20, optY0 + (optH + optGap) + 12, "Discard Changes");
    discardText->SetFont(pksm::ui::global::MakeMediumFontName(28));
    discardText->SetColor(OPTION_TEXT);
    this->Add(discardText);

    cancelText = pu::ui::elm::TextBlock::New(optX + 20, optY0 + (2 * (optH + optGap)) + 12, "Cancel");
    cancelText->SetFont(pksm::ui::global::MakeMediumFontName(28));
    cancelText->SetColor(OPTION_TEXT);
    this->Add(cancelText);

    SetSelectedIndex(selectedIndex);
}

}  // namespace pksm::ui