#pragma once

#include <functional>
#include <pu/Plutonium>

#include "data/providers/interfaces/IBoxDataProvider.hpp"
#include "data/providers/interfaces/ISaveDataAccessor.hpp"
#include "data/providers/interfaces/IPartyDataProvider.hpp"
#include "gui/screens/editor-screen/EditorActionOverlay.hpp"
#include "gui/screens/editor-screen/PokemonEditOverlay.hpp"
#include "gui/shared/components/AnimatedBackground.hpp"
#include "gui/shared/components/BaseLayout.hpp"
#include "gui/shared/components/PokemonBox.hpp"
#include "gui/shared/components/PokemonSummaryOverlay.hpp"
#include "input/ButtonInputHandler.hpp"
#include "input/visual-feedback/FocusManager.hpp"
#include "input/visual-feedback/SelectionManager.hpp"

namespace pksm::layout {

class EditorScreen : public BaseLayout {
public:
    EditorScreen(
        std::function<void()> onBack,
        std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
        std::function<void()> onHideOverlay,
        ISaveDataAccessor::Ref saveDataAccessor,
        IBoxDataProvider::Ref boxDataProvider,
        IPartyDataProvider::Ref partyDataProvider
    );
    PU_SMART_CTOR(EditorScreen)
    ~EditorScreen();

    // Called externally when save data changes (e.g. new save loaded)
    void LoadData();

private:
    enum class ActivePanel { Boxes, Team };

    std::function<void()> onBack;
    pksm::input::ButtonInputHandler buttonHandler;

    ISaveDataAccessor::Ref saveDataAccessor;
    IBoxDataProvider::Ref boxDataProvider;
    IPartyDataProvider::Ref partyDataProvider;

    pksm::input::FocusManager::Ref rootFocusManager;
    pksm::input::SelectionManager::Ref rootSelectionManager;
    pksm::input::FocusManager::Ref boxesFocusManager;
    pksm::input::SelectionManager::Ref boxesSelectionManager;
    pksm::input::FocusManager::Ref teamFocusManager;
    pksm::input::SelectionManager::Ref teamSelectionManager;

    // Background
    pu::ui::elm::Element::Ref background;

    // ── Left panel: Pokémon info ──────────────────────────────────────────
    pu::ui::elm::Rectangle::Ref infoPanelBg;
    pu::ui::elm::Rectangle::Ref infoHeaderBar;
    // Highlight bar that moves to the active inline-edit field
    pu::ui::elm::Rectangle::Ref infoEditHighlight;

    // Header: species name + gender + level
    pu::ui::elm::TextBlock::Ref infoSpeciesText;
    pu::ui::elm::TextBlock::Ref infoGenderText;
    pu::ui::elm::TextBlock::Ref infoLevelText;

    // Left column: Nickname, OT, Pokérus, Nature, Ability, Item, Hidden Power
    pu::ui::elm::TextBlock::Ref infoNicknameLabel;  pu::ui::elm::TextBlock::Ref infoNicknameVal;
    pu::ui::elm::TextBlock::Ref infoOTLabel;        pu::ui::elm::TextBlock::Ref infoOTVal;
    pu::ui::elm::TextBlock::Ref infoPokerusLabel;   pu::ui::elm::TextBlock::Ref infoPokerusVal;
    pu::ui::elm::TextBlock::Ref infoNatureLabel;    pu::ui::elm::TextBlock::Ref infoNatureVal;
    pu::ui::elm::TextBlock::Ref infoAbilityLabel;   pu::ui::elm::TextBlock::Ref infoAbilityVal;
    pu::ui::elm::TextBlock::Ref infoItemLabel;      pu::ui::elm::TextBlock::Ref infoItemVal;
    pu::ui::elm::TextBlock::Ref infoHpTypeLabel;    pu::ui::elm::TextBlock::Ref infoHpTypeVal;

    // Right sub-column: stats (HP/Atk/Def/SpAtk/SpDef/Spd) with IV EV STAT headers
    pu::ui::elm::TextBlock::Ref statsHeaderIV;
    pu::ui::elm::TextBlock::Ref statsHeaderEV;
    pu::ui::elm::TextBlock::Ref statsHeaderST;
    // Per stat: label, iv, ev, stat
    pu::ui::elm::TextBlock::Ref statHpLbl;   pu::ui::elm::TextBlock::Ref statHpIV;   pu::ui::elm::TextBlock::Ref statHpEV;   pu::ui::elm::TextBlock::Ref statHpST;
    pu::ui::elm::TextBlock::Ref statAtkLbl;  pu::ui::elm::TextBlock::Ref statAtkIV;  pu::ui::elm::TextBlock::Ref statAtkEV;  pu::ui::elm::TextBlock::Ref statAtkST;
    pu::ui::elm::TextBlock::Ref statDefLbl;  pu::ui::elm::TextBlock::Ref statDefIV;  pu::ui::elm::TextBlock::Ref statDefEV;  pu::ui::elm::TextBlock::Ref statDefST;
    pu::ui::elm::TextBlock::Ref statSpaLbl;  pu::ui::elm::TextBlock::Ref statSpaIV;  pu::ui::elm::TextBlock::Ref statSpaEV;  pu::ui::elm::TextBlock::Ref statSpaST;
    pu::ui::elm::TextBlock::Ref statSpdLbl;  pu::ui::elm::TextBlock::Ref statSpdIV;  pu::ui::elm::TextBlock::Ref statSpdEV;  pu::ui::elm::TextBlock::Ref statSpdST;
    pu::ui::elm::TextBlock::Ref statSpeeLbl; pu::ui::elm::TextBlock::Ref statSpeeIV; pu::ui::elm::TextBlock::Ref statSpeeEV; pu::ui::elm::TextBlock::Ref statSpeeST;

    // Moves section
    pu::ui::elm::TextBlock::Ref movesHeader;
    pu::ui::elm::TextBlock::Ref moveSlot1;
    pu::ui::elm::TextBlock::Ref moveSlot2;
    pu::ui::elm::TextBlock::Ref moveSlot3;
    pu::ui::elm::TextBlock::Ref moveSlot4;

    // Divider between left and right panels
    pu::ui::elm::Rectangle::Ref panelDivider;

    // ── Right panel: title + boxes + team ────────────────────────────────
    pu::ui::elm::TextBlock::Ref titleText;

    pksm::ui::PokemonBox::Ref boxesPanel;
    pksm::ui::PokemonBox::Ref teamPanel;

    // ── Overlays ─────────────────────────────────────────────────────────
    pksm::ui::EditorActionOverlay::Ref actionOverlay;
    pksm::ui::PokemonEditOverlay::Ref pokemonEditOverlay;
    bool actionOverlayVisible = false;
    bool summaryOverlayVisible = false;
    bool pokemonEditOverlayVisible = false;
    bool suppressPanelSelectionSync = false;

    // ── State ─────────────────────────────────────────────────────────────
    ActivePanel activePanel = ActivePanel::Boxes;
    struct GrabState { bool active=false; ActivePanel panel=ActivePanel::Boxes; int box=0; int slot=0; };
    GrabState grab;

    enum class ContextAction { Edit, Move, Clone, Delete };

    bool inlineEditOpen = false;
    ActivePanel editPanel = ActivePanel::Boxes;
    int editBox = 0;
    int editSlot = 0;
    int editSpecies = 0;
    int editLevel = 1;
    bool editShiny = false;
    int editNature = 0;
    int editAbility = 1;
    int editMove1 = 0;
    int editMove2 = 0;
    int editMove3 = 0;
    int editMove4 = 0;
    int editField = 0;

    void OnInput(u64 down, u64 up, u64 held);
    void SetActivePanel(ActivePanel panel);
    void HandlePrimaryAction();
    void OpenActionMenu();
    void ExecuteContextAction(ContextAction action);
    void OpenInlineEdit();
    void HandleInlineEditInput(u64 down);
    void CommitInlineEdit();
    void OpenPokemonEditOverlay();
    void CommitPokemonEditOverlay();
    void ClosePokemonEditOverlay();
    void OpenSummaryOverlay();
    void UpdateEditHighlight();  // move the info-panel highlight to editField row

    // Refresh the left info panel for a given PKX (pass nullptr to clear)
    void RefreshInfoPanel(const pksm::PKX* pk);
    void ClearInfoPanel();

    std::vector<pksm::ui::HelpItem> GetHelpOverlayItems() const override;
};

} // namespace pksm::layout
