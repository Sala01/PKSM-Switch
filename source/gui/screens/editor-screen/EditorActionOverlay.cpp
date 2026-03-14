#include "gui/screens/editor-screen/EditorActionOverlay.hpp"
#include "gui/shared/UIConstants.hpp"

namespace pksm::ui {

EditorActionOverlay::EditorActionOverlay(pu::i32 x, pu::i32 y, pu::i32 width, pu::i32 height)
  : pu::ui::Overlay(x, y, width, height, OVERLAY_BG),
    titleStr("Actions"),
    actions({"Edit", "Move", "Clone", "Delete"}),
    selectedIndex(0) {
    this->SetRadius(0);
    this->SetMaxFadeAlpha(200);
    this->SetFadeAlphaVariation(18);
    Rebuild();
}

void EditorActionOverlay::SetTitle(const std::string& title) {
    titleStr = title;
    if (titleText) {
        titleText->SetText(titleStr);
    }
}

void EditorActionOverlay::SetActions(const std::vector<std::string>& acts) {
    actions = acts;
    selectedIndex = 0;
    Rebuild();
}

void EditorActionOverlay::MoveUp() {
    if (actions.empty()) return;
    SetSelectedIndex((selectedIndex + static_cast<int>(actions.size()) - 1) % static_cast<int>(actions.size()));
}

void EditorActionOverlay::MoveDown() {
    if (actions.empty()) return;
    SetSelectedIndex((selectedIndex + 1) % static_cast<int>(actions.size()));
}

const std::string& EditorActionOverlay::GetSelectedAction() const {
    static const std::string empty;
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(actions.size())) return empty;
    return actions[selectedIndex];
}

void EditorActionOverlay::SetSelectedIndex(int idx) {
    if (actions.empty()) return;
    selectedIndex = idx;

    if (!optionHighlight) return;

    const pu::i32 panelH = HDR_H + static_cast<pu::i32>(actions.size()) * (OPT_H + OPT_GAP) + 30;
    const pu::i32 panelX = (this->GetWidth()  - PANEL_W) / 2;
    const pu::i32 panelY = (this->GetHeight() - panelH)  / 2;

    const pu::i32 optX = panelX + PANEL_PAD_X;
    const pu::i32 optY0 = panelY + HDR_H;
    const pu::i32 optW  = PANEL_W - 2 * PANEL_PAD_X;

    optionHighlight->SetX(optX);
    optionHighlight->SetY(optY0 + selectedIndex * (OPT_H + OPT_GAP));
    optionHighlight->SetWidth(optW);
}

void EditorActionOverlay::Rebuild() {
    this->Clear();
    optionTexts.clear();

    const int n = static_cast<int>(actions.size());
    const pu::i32 panelH = HDR_H + n * (OPT_H + OPT_GAP) + 30;
    const pu::i32 panelX = (this->GetWidth()  - PANEL_W) / 2;
    const pu::i32 panelY = (this->GetHeight() - panelH)  / 2;
    const pu::i32 optX   = panelX + PANEL_PAD_X;
    const pu::i32 optY0  = panelY + HDR_H;
    const pu::i32 optW   = PANEL_W - 2 * PANEL_PAD_X;

    // Drop shadow
    panelShadow = pu::ui::elm::Rectangle::New(panelX + 8, panelY + 8, PANEL_W, panelH, SHADOW_COL);
    panelShadow->SetBorderRadius(PANEL_RADIUS);
    this->Add(panelShadow);

    // Main panel
    panel = pu::ui::elm::Rectangle::New(panelX, panelY, PANEL_W, panelH, PANEL_BG);
    panel->SetBorderRadius(PANEL_RADIUS);
    this->Add(panel);

    // Title
    titleText = pu::ui::elm::TextBlock::New(panelX + PANEL_PAD_X, panelY + 22, titleStr);
    titleText->SetFont(pksm::ui::global::MakeHeavyFontName(28));
    titleText->SetColor(TITLE_COL);
    this->Add(titleText);

    // Selection highlight (placed behind option texts)
    optionHighlight = pu::ui::elm::Rectangle::New(optX, optY0, optW, OPT_H, HL_COL);
    optionHighlight->SetBorderRadius(14);
    this->Add(optionHighlight);

    // Option labels
    for (int i = 0; i < n; i++) {
        const pu::i32 lblY = optY0 + i * (OPT_H + OPT_GAP) + (OPT_H - 28) / 2;
        auto lbl = pu::ui::elm::TextBlock::New(optX + 18, lblY, actions[i]);
        lbl->SetFont(pksm::ui::global::MakeMediumFontName(26));
        lbl->SetColor(OPT_TEXT_COL);
        optionTexts.push_back(lbl);
        this->Add(lbl);
    }

    // Position the highlight correctly
    SetSelectedIndex(selectedIndex);
}

}  // namespace pksm::ui
