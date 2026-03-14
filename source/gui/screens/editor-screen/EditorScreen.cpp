#include "gui/screens/editor-screen/EditorScreen.hpp"

#include <format>

#include "gui/shared/UIConstants.hpp"
#include "pksmcore/utils/i18n.hpp"
#include "utils/Logger.hpp"
#include "utils/NotificationManager.hpp"

namespace pksm::layout {

// ─── Layout constants ──────────────────────────────────────────────────────────
// Layout now scales from the original 1280x720 design space to the actual
// runtime layout size and anchors the right panels to the right margin so the
// screen width is fully utilized (matching StorageScreen behavior).
static constexpr pu::i32 BASE_LAYOUT_W = 1280;
static constexpr pu::i32 BASE_LAYOUT_H = 720;
static constexpr pu::i32 SCREEN_W = static_cast<pu::i32>(pu::ui::render::ScreenWidth);
static constexpr pu::i32 SCREEN_H = static_cast<pu::i32>(pu::ui::render::ScreenHeight);

constexpr pu::i32 ScaleX(const pu::i32 value) {
    return (value * SCREEN_W) / BASE_LAYOUT_W;
}

constexpr pu::i32 ScaleY(const pu::i32 value) {
    return (value * SCREEN_H) / BASE_LAYOUT_H;
}

static constexpr pu::i32 SIDE_MARGIN   = ScaleX(20);

// Header
static constexpr pu::i32 HEADER_TEXT_X = ScaleX(70);
static constexpr pu::i32 HEADER_TEXT_Y = ScaleY(35);

static constexpr pu::i32 TOP_Y         = ScaleY(100);
static constexpr pu::i32 PANEL_GAP     = ScaleX(20);

// ── Box panel (RIGHT) ──────────────────────────────────────────────────────────
// Keep a storage-like footprint on 1080p (item ~=124) while remaining scalable.
static constexpr pu::i32 BOX_ITEM      = ScaleX(83);
static constexpr pu::i32 BOX_ROWS      = 5;
static constexpr pu::i32 BOX_COLS      = 6;
static constexpr pu::i32 BOX_W         = BOX_COLS * BOX_ITEM + (BOX_COLS - 1) * 8 + 20;
static constexpr pu::i32 BOX_X         = SCREEN_W - SIDE_MARGIN - BOX_W;
static constexpr pu::i32 BOX_Y         = TOP_Y;

// ── Team panel (between info and box) ─────────────────────────────────────────
static constexpr pu::i32 TEAM_ITEM     = ScaleX(110);
static constexpr pu::i32 TEAM_ROWS     = 3;
static constexpr pu::i32 TEAM_COLS     = 2;
static constexpr pu::i32 TEAM_TOP_FRAME_H = ScaleY(88);
static constexpr pu::i32 TEAM_BOTTOM_FRAME_H = 0;
static constexpr pu::i32 TEAM_W        = TEAM_COLS * TEAM_ITEM + (TEAM_COLS - 1) * 8 + 20;
static constexpr pu::i32 TEAM_X        = BOX_X - PANEL_GAP - TEAM_W;
static constexpr pu::i32 TEAM_Y        = TOP_Y;

// ── Info panel (LEFT) ──────────────────────────────────────────────────────────
static constexpr pu::i32 INFO_X        = ScaleX(30);
static constexpr pu::i32 INFO_Y        = TOP_Y;
static constexpr pu::i32 INFO_W        = TEAM_X - PANEL_GAP - INFO_X;
static constexpr pu::i32 INFO_H        = SCREEN_H - TOP_Y - ScaleY(10);
static constexpr pu::i32 INFO_PAD      = ScaleX(14);
static constexpr pu::i32 INFO_HDR_H    = ScaleY(60);
static constexpr pu::i32 ROW_H         = ScaleY(26);
static constexpr pu::i32 STAT_ROW_H    = ScaleY(24);
static constexpr pu::i32 MOVE_ROW_H    = ScaleY(22);
static constexpr pu::i32 MOVES_HDR_H   = ScaleY(18);

// Text column X positions inside the info panel
static constexpr pu::i32 LBL_X         = INFO_X + INFO_PAD;
static constexpr pu::i32 VAL_X         = INFO_X + ScaleX(160);
static constexpr pu::i32 STAT_LBL_X    = INFO_X + INFO_PAD;
static constexpr pu::i32 STAT_IV_X     = INFO_X + ScaleX(110);
static constexpr pu::i32 STAT_EV_X     = INFO_X + ScaleX(200);
static constexpr pu::i32 STAT_ST_X     = INFO_X + ScaleX(290);

// ─── Colors ────────────────────────────────────────────────────────────────────
static constexpr pu::ui::Color COL_INFO_BG   = pu::ui::Color( 20,  30,  65, 230);
static constexpr pu::ui::Color COL_INFO_HDR  = pu::ui::Color( 42,  68, 148, 255);
static constexpr pu::ui::Color COL_LABEL     = pu::ui::Color(160, 185, 255, 255);
static constexpr pu::ui::Color COL_VALUE     = pu::ui::Color(245, 248, 255, 255);
static constexpr pu::ui::Color COL_STAT_HP   = pu::ui::Color( 80, 220,  80, 255);
static constexpr pu::ui::Color COL_STAT_ATK  = pu::ui::Color(255, 100,  80, 255);
static constexpr pu::ui::Color COL_STAT_DEF  = pu::ui::Color(100, 160, 255, 255);
static constexpr pu::ui::Color COL_STAT_SPA  = pu::ui::Color(200, 100, 255, 255);
static constexpr pu::ui::Color COL_STAT_SPD  = pu::ui::Color(255, 210,  50, 255);
static constexpr pu::ui::Color COL_STAT_SPE  = pu::ui::Color( 50, 210, 210, 255);
static constexpr pu::ui::Color COL_MOVE_HDR  = pu::ui::Color(210, 165,  50, 255);
static constexpr pu::ui::Color COL_MOVE      = pu::ui::Color(230, 225, 185, 255);

// ─── Fonts ─────────────────────────────────────────────────────────────────────
static const std::string FONT_TITLE = pksm::ui::global::MakeHeavyFontName(pksm::ui::global::FONT_SIZE_TITLE);
static const std::string FONT_HDR   = pksm::ui::global::MakeHeavyFontName(22);
static const std::string FONT_LBL   = pksm::ui::global::MakeHeavyFontName(18);
static const std::string FONT_VAL   = pksm::ui::global::MakeMediumFontName(18);
static const std::string FONT_STAT  = pksm::ui::global::MakeMediumFontName(17);
static const std::string FONT_STATL = pksm::ui::global::MakeHeavyFontName(17);
static const std::string FONT_MOVE  = pksm::ui::global::MakeMediumFontName(19);
static const std::string FONT_MOVH  = pksm::ui::global::MakeHeavyFontName(19);

// ─── i18n helpers ──────────────────────────────────────────────────────────────

static void EnsureI18n() {
    static bool inited = false;
    if (!inited) { i18n::init(pksm::Language::ENG); inited = true; }
}

static std::string SafeStr(const std::string& s, const std::string& fallback = "\xe2\x80\x94") {
    return s.empty() ? fallback : s;
}

static std::string SpeciesName(const pksm::PKX& pk) {
    EnsureI18n();
    return SafeStr(i18n::species(pksm::Language::ENG, pk.species()), "???");
}

static std::string NatureName(const pksm::PKX& pk) {
    EnsureI18n();
    return SafeStr(i18n::nature(pksm::Language::ENG, pk.nature()));
}

static std::string AbilityName(const pksm::PKX& pk) {
    EnsureI18n();
    return SafeStr(i18n::ability(pksm::Language::ENG, pk.ability()));
}

static std::string ItemName(const pksm::PKX& pk) {
    EnsureI18n();
    if (pk.heldItem() == 0) return "None";
    return SafeStr(i18n::item(pksm::Language::ENG, pk.heldItem()));
}

static std::string MoveName(const pksm::PKX& pk, u8 idx) {
    EnsureI18n();
    auto m = pk.move(idx);
    if (m == pksm::Move::None) return "\xe2\x80\x94";
    return SafeStr(i18n::move(pksm::Language::ENG, m));
}

static std::string GenderStr(pksm::Gender g) {
    if (g == pksm::Gender::Male)   return " \xe2\x99\x82";
    if (g == pksm::Gender::Female) return " \xe2\x99\x80";
    return "";
}

static std::string HpTypeName(const pksm::PKX& pk) {
    EnsureI18n();
    return SafeStr(i18n::type(pksm::Language::ENG, pk.hpType()));
}

// ─── Element factory helpers ───────────────────────────────────────────────────

static pu::ui::elm::TextBlock::Ref MakeLbl(pu::i32 x, pu::i32 y, const std::string& text) {
    auto t = pu::ui::elm::TextBlock::New(x, y, text);
    t->SetColor(COL_LABEL);
    t->SetFont(FONT_LBL);
    return t;
}

static pu::ui::elm::TextBlock::Ref MakeVal(pu::i32 x, pu::i32 y, const std::string& text) {
    auto t = pu::ui::elm::TextBlock::New(x, y, text);
    t->SetColor(COL_VALUE);
    t->SetFont(FONT_VAL);
    return t;
}

static pu::ui::elm::TextBlock::Ref MakeStat(
    pu::i32 x, pu::i32 y, const std::string& text, const pu::ui::Color& col)
{
    auto t = pu::ui::elm::TextBlock::New(x, y, text);
    t->SetColor(col);
    t->SetFont(FONT_STATL);
    return t;
}

static pu::ui::elm::TextBlock::Ref MakeStatVal(pu::i32 x, pu::i32 y, const std::string& text) {
    auto t = pu::ui::elm::TextBlock::New(x, y, text);
    t->SetColor(COL_VALUE);
    t->SetFont(FONT_STAT);
    return t;
}

// ─── Constructor ───────────────────────────────────────────────────────────────

EditorScreen::EditorScreen(
    std::function<void()> onBack,
    std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
    std::function<void()> onHideOverlay,
    ISaveDataAccessor::Ref saveDataAccessor,
    IBoxDataProvider::Ref boxDataProvider,
    IPartyDataProvider::Ref partyDataProvider
)
  : BaseLayout(onShowOverlay, onHideOverlay),
    onBack(onBack),
    buttonHandler(),
    saveDataAccessor(saveDataAccessor),
    boxDataProvider(boxDataProvider),
    partyDataProvider(partyDataProvider)
{
    LOG_DEBUG("Initializing EditorScreen...");

    // Set background colour before adding the animated background
    // (same pattern as StorageScreen, BagScreen, MainMenu, etc.)
    this->SetBackgroundColor(pksm::ui::global::BACKGROUND_BLUE);

    // Animated background (same pattern as StorageScreen)
    background = ui::AnimatedBackground::New();
    this->Add(background);

    // ── Header ──────────────────────────────────────────────────────────────
    titleText = pu::ui::elm::TextBlock::New(HEADER_TEXT_X, HEADER_TEXT_Y, "Editor");
    titleText->SetColor(pksm::ui::global::TEXT_WHITE);
    titleText->SetFont(FONT_TITLE);
    this->Add(titleText);

    // ── Focus / selection managers ──────────────────────────────────────────
    rootFocusManager = pksm::input::FocusManager::New("EditorRoot");
    rootSelectionManager = pksm::input::SelectionManager::New("EditorRoot");
    rootFocusManager->SetActive(true);
    rootSelectionManager->SetActive(true);

    boxesFocusManager = pksm::input::FocusManager::New("EditorBoxes");
    boxesSelectionManager = pksm::input::SelectionManager::New("EditorBoxes");
    teamFocusManager = pksm::input::FocusManager::New("EditorTeam");
    teamSelectionManager = pksm::input::SelectionManager::New("EditorTeam");

    rootFocusManager->RegisterChildManager(boxesFocusManager);
    rootFocusManager->RegisterChildManager(teamFocusManager);
    rootSelectionManager->RegisterChildManager(boxesSelectionManager);
    rootSelectionManager->RegisterChildManager(teamSelectionManager);

    // ── BOX panel (right — Pokémon storage) ────────────────────────────────
    boxesPanel = pksm::ui::PokemonBox::New(
        BOX_X, BOX_Y, BOX_ITEM,
        boxesFocusManager, boxesSelectionManager,
        std::map<pksm::ui::ShakeDirection, bool>{},
        BOX_ROWS, BOX_COLS
    );
    boxesPanel->SetName("EditorBoxesPanel");
    boxesPanel->EstablishOwningRelationship();
    this->Add(boxesPanel);

    // ── INFO panel background (left — Pokémon details) ─────────────────────
    infoPanelBg = pu::ui::elm::Rectangle::New(INFO_X, INFO_Y, INFO_W, INFO_H, COL_INFO_BG, 12);
    this->Add(infoPanelBg);

    infoHeaderBar = pu::ui::elm::Rectangle::New(INFO_X, INFO_Y, INFO_W, INFO_HDR_H, COL_INFO_HDR, 12);
    this->Add(infoHeaderBar);

    // Edit-mode field highlight (hidden until inline edit is open)
    infoEditHighlight = pu::ui::elm::Rectangle::New(
        INFO_X + 4, INFO_Y + INFO_HDR_H, INFO_W - 8, ROW_H,
        pu::ui::Color(100, 160, 255, 60), 6);
    infoEditHighlight->SetVisible(false);
    this->Add(infoEditHighlight);

    // Header: species + gender + level
    const pu::i32 hdrMidY = INFO_Y + (INFO_HDR_H - 24) / 2;
    infoSpeciesText = pu::ui::elm::TextBlock::New(LBL_X, hdrMidY, "\xe2\x80\x94");
    infoSpeciesText->SetColor(COL_VALUE);
    infoSpeciesText->SetFont(FONT_HDR);
    this->Add(infoSpeciesText);

    infoGenderText = pu::ui::elm::TextBlock::New(LBL_X + 180, hdrMidY, "");
    infoGenderText->SetColor(COL_VALUE);
    infoGenderText->SetFont(FONT_HDR);
    this->Add(infoGenderText);

    infoLevelText = pu::ui::elm::TextBlock::New(LBL_X + 230, hdrMidY, "");
    infoLevelText->SetColor(pu::ui::Color(200, 255, 200, 255));
    infoLevelText->SetFont(FONT_HDR);
    this->Add(infoLevelText);

    // Info rows
    pu::i32 rowY = INFO_Y + INFO_HDR_H + 8;

    auto addRow = [&](pu::ui::elm::TextBlock::Ref& lbl, pu::ui::elm::TextBlock::Ref& val,
                      const std::string& lblText, const std::string& valText) {
        lbl = MakeLbl(LBL_X, rowY, lblText);
        val = MakeVal(VAL_X, rowY, valText);
        this->Add(lbl);
        this->Add(val);
        rowY += ROW_H;
    };

    addRow(infoNicknameLabel, infoNicknameVal, "Nickname",    "\xe2\x80\x94");
    addRow(infoOTLabel,       infoOTVal,       "OT",          "\xe2\x80\x94");
    addRow(infoPokerusLabel,  infoPokerusVal,  "Poke\xcc\x81rus",   "\xe2\x80\x94");
    addRow(infoNatureLabel,   infoNatureVal,   "Nature",      "\xe2\x80\x94");
    addRow(infoAbilityLabel,  infoAbilityVal,  "Ability",     "\xe2\x80\x94");
    addRow(infoItemLabel,     infoItemVal,     "Item",        "\xe2\x80\x94");
    addRow(infoHpTypeLabel,   infoHpTypeVal,   "HP Type",     "\xe2\x80\x94");

    rowY += 6;

    // Stats header
    statsHeaderIV = pu::ui::elm::TextBlock::New(STAT_IV_X, rowY, "IV");
    statsHeaderIV->SetColor(COL_LABEL); statsHeaderIV->SetFont(FONT_LBL);
    this->Add(statsHeaderIV);

    statsHeaderEV = pu::ui::elm::TextBlock::New(STAT_EV_X, rowY, "EV");
    statsHeaderEV->SetColor(COL_LABEL); statsHeaderEV->SetFont(FONT_LBL);
    this->Add(statsHeaderEV);

    statsHeaderST = pu::ui::elm::TextBlock::New(STAT_ST_X, rowY, "ST");
    statsHeaderST->SetColor(COL_LABEL); statsHeaderST->SetFont(FONT_LBL);
    this->Add(statsHeaderST);

    rowY += STAT_ROW_H - 2;

    auto addStatRow = [&](pu::ui::elm::TextBlock::Ref& lbl, pu::ui::elm::TextBlock::Ref& iv,
                          pu::ui::elm::TextBlock::Ref& ev, pu::ui::elm::TextBlock::Ref& st,
                          const std::string& name, const pu::ui::Color& col) {
        lbl = MakeStat(STAT_LBL_X, rowY, name, col);
        iv  = MakeStatVal(STAT_IV_X,  rowY, "\xe2\x80\x94");
        ev  = MakeStatVal(STAT_EV_X,  rowY, "\xe2\x80\x94");
        st  = MakeStatVal(STAT_ST_X,  rowY, "\xe2\x80\x94");
        this->Add(lbl); this->Add(iv); this->Add(ev); this->Add(st);
        rowY += STAT_ROW_H;
    };

    addStatRow(statHpLbl,   statHpIV,   statHpEV,   statHpST,   "HP",  COL_STAT_HP);
    addStatRow(statAtkLbl,  statAtkIV,  statAtkEV,  statAtkST,  "Atk", COL_STAT_ATK);
    addStatRow(statDefLbl,  statDefIV,  statDefEV,  statDefST,  "Def", COL_STAT_DEF);
    addStatRow(statSpaLbl,  statSpaIV,  statSpaEV,  statSpaST,  "SpA", COL_STAT_SPA);
    addStatRow(statSpdLbl,  statSpdIV,  statSpdEV,  statSpdST,  "SpD", COL_STAT_SPD);
    addStatRow(statSpeeLbl, statSpeeIV, statSpeeEV, statSpeeST, "Spe", COL_STAT_SPE);

    rowY += 6;

    // Moves
    movesHeader = pu::ui::elm::TextBlock::New(LBL_X, rowY, "Moves");
    movesHeader->SetColor(COL_MOVE_HDR);
    movesHeader->SetFont(FONT_MOVH);
    this->Add(movesHeader);
    rowY += MOVES_HDR_H;

    moveSlot1 = pu::ui::elm::TextBlock::New(LBL_X + 10, rowY, "\xe2\x80\x94");
    moveSlot1->SetColor(COL_MOVE); moveSlot1->SetFont(FONT_MOVE);
    this->Add(moveSlot1); rowY += MOVE_ROW_H;

    moveSlot2 = pu::ui::elm::TextBlock::New(LBL_X + 10, rowY, "\xe2\x80\x94");
    moveSlot2->SetColor(COL_MOVE); moveSlot2->SetFont(FONT_MOVE);
    this->Add(moveSlot2); rowY += MOVE_ROW_H;

    moveSlot3 = pu::ui::elm::TextBlock::New(LBL_X + 10, rowY, "\xe2\x80\x94");
    moveSlot3->SetColor(COL_MOVE); moveSlot3->SetFont(FONT_MOVE);
    this->Add(moveSlot3); rowY += MOVE_ROW_H;

    moveSlot4 = pu::ui::elm::TextBlock::New(LBL_X + 10, rowY, "\xe2\x80\x94");
    moveSlot4->SetColor(COL_MOVE); moveSlot4->SetFont(FONT_MOVE);
    this->Add(moveSlot4);

    // Thin divider line between info panel and right panels
    panelDivider = pu::ui::elm::Rectangle::New(
        INFO_X + INFO_W + PANEL_GAP / 2 - 2, TOP_Y, 4, SCREEN_H - TOP_Y,
        pu::ui::Color(70, 110, 220, 160), 0);
    this->Add(panelDivider);

    // ── TEAM panel (top-right — party slots) ───────────────────────────────
    teamPanel = pksm::ui::PokemonBox::New(
        TEAM_X, TEAM_Y, TEAM_ITEM,
        teamFocusManager, teamSelectionManager,
        std::map<pksm::ui::ShakeDirection, bool>{},
        TEAM_ROWS, TEAM_COLS,
        TEAM_TOP_FRAME_H, TEAM_BOTTOM_FRAME_H
    );
    teamPanel->SetName("EditorTeamPanel");
    teamPanel->EstablishOwningRelationship();
    teamPanel->SetNavigationControlsVisible(false);
    teamPanel->SetFooterControlsVisible(false);
    this->Add(teamPanel);

    // ── Selection callbacks → refresh info panel ────────────────────────────
    boxesPanel->SetOnSelectionChanged([this](int box, int slot) {
        if (!suppressPanelSelectionSync && !actionOverlayVisible &&
            !inlineEditOpen && !pokemonEditOverlayVisible && activePanel != ActivePanel::Boxes) {
            SetActivePanel(ActivePanel::Boxes);
            return;
        }

        auto sd = this->saveDataAccessor ? this->saveDataAccessor->getCurrentSaveData() : nullptr;
        if (!sd || !this->boxDataProvider) { ClearInfoPanel(); return; }
        auto pk = this->boxDataProvider->GetPokemon(sd, box, slot);
        RefreshInfoPanel(pk ? pk.get() : nullptr);
    });

    teamPanel->SetOnSelectionChanged([this](int /*box*/, int slot) {
        if (!suppressPanelSelectionSync && !actionOverlayVisible &&
            !inlineEditOpen && !pokemonEditOverlayVisible && activePanel != ActivePanel::Team) {
            SetActivePanel(ActivePanel::Team);
            return;
        }

        auto sd = this->saveDataAccessor ? this->saveDataAccessor->getCurrentSaveData() : nullptr;
        if (!sd || !this->partyDataProvider) { ClearInfoPanel(); return; }
        auto pk = this->partyDataProvider->GetPartyPokemon(sd, slot);
        RefreshInfoPanel(pk ? pk.get() : nullptr);
    });

    // ── Action overlay ──────────────────────────────────────────────────────
    actionOverlay = pksm::ui::EditorActionOverlay::New(0, 0, GetWidth(), GetHeight());
    pokemonEditOverlay = pksm::ui::PokemonEditOverlay::New(0, 0, GetWidth(), GetHeight());

    // ── Input handlers ──────────────────────────────────────────────────────
    // D-pad and L/R navigation are handled by each PokemonBox internally.
    // X toggles which panel is active (Boxes <-> Team).
    buttonHandler.RegisterButton(HidNpadButton_B, nullptr, [this]() {
        if (this->onBack) this->onBack();
    });
    buttonHandler.RegisterButton(HidNpadButton_A, nullptr, [this]() { HandlePrimaryAction(); });
    buttonHandler.RegisterButton(HidNpadButton_X, nullptr, [this]() {
        const auto nextPanel = (activePanel == ActivePanel::Boxes) ? ActivePanel::Team : ActivePanel::Boxes;
        SetActivePanel(nextPanel);
    });

    // ── Help footer ─────────────────────────────────────────────────────────
    InitializeHelpFooter();
    helpFooter->SetHelpItems({
        {{{pksm::ui::global::ButtonGlyph::DPad}},                                          "Navigate"},
        {{{pksm::ui::global::ButtonGlyph::A}},                                             "Action Menu"},
        {{{pksm::ui::global::ButtonGlyph::X}},                                             "Switch Box/Team"},
        {{{pksm::ui::global::ButtonGlyph::L}, {pksm::ui::global::ButtonGlyph::R}},         "Switch Box"},
        {{{pksm::ui::global::ButtonGlyph::B}},                                             "Back"}
    });

    SetActivePanel(ActivePanel::Boxes);
    LoadData();

    this->SetOnInput(
        std::bind(&EditorScreen::OnInput, this, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3)
    );

    LOG_DEBUG("EditorScreen initialization complete");
    // *** Critical — same as StorageScreen: initialise all element textures ***
    PreRender();
}

EditorScreen::~EditorScreen() = default;

// ─── Info panel ────────────────────────────────────────────────────────────────

void EditorScreen::RefreshInfoPanel(const pksm::PKX* pk) {
    if (!pk || static_cast<int>(pk->species()) == 0) {
        ClearInfoPanel();
        return;
    }

    infoSpeciesText->SetText(SpeciesName(*pk) + (pk->shiny() ? " \xe2\x98\x85" : "") + GenderStr(pk->gender()));
    infoGenderText->SetText("");
    infoLevelText->SetText("Lv." + std::to_string(pk->level()));

    infoNicknameVal->SetText(SafeStr(pk->nickname()));
    infoOTVal->SetText(SafeStr(pk->otName()));
    infoPokerusVal->SetText(pk->pkrsDays() > 0 ? "Active" : (pk->pkrsStrain() > 0 ? "Cured" : "No"));
    infoNatureVal->SetText(NatureName(*pk));
    infoAbilityVal->SetText(AbilityName(*pk));
    infoItemVal->SetText(ItemName(*pk));
    infoHpTypeVal->SetText(HpTypeName(*pk));

    auto setStat = [&](pu::ui::elm::TextBlock::Ref& iv, pu::ui::elm::TextBlock::Ref& ev,
                       pu::ui::elm::TextBlock::Ref& st, pksm::Stat s) {
        iv->SetText(std::to_string(pk->iv(s)));
        ev->SetText(std::to_string(pk->ev(s)));
        st->SetText(std::to_string(pk->stat(s)));
    };
    setStat(statHpIV,   statHpEV,   statHpST,   pksm::Stat::HP);
    setStat(statAtkIV,  statAtkEV,  statAtkST,  pksm::Stat::ATK);
    setStat(statDefIV,  statDefEV,  statDefST,  pksm::Stat::DEF);
    setStat(statSpaIV,  statSpaEV,  statSpaST,  pksm::Stat::SPATK);
    setStat(statSpdIV,  statSpdEV,  statSpdST,  pksm::Stat::SPDEF);
    setStat(statSpeeIV, statSpeeEV, statSpeeST, pksm::Stat::SPD);

    moveSlot1->SetText("\xc2\xb7 " + MoveName(*pk, 0));
    moveSlot2->SetText("\xc2\xb7 " + MoveName(*pk, 1));
    moveSlot3->SetText("\xc2\xb7 " + MoveName(*pk, 2));
    moveSlot4->SetText("\xc2\xb7 " + MoveName(*pk, 3));
}

void EditorScreen::ClearInfoPanel() {
    const std::string dash = "\xe2\x80\x94";

    infoSpeciesText->SetText(dash);
    infoGenderText->SetText("");
    infoLevelText->SetText("");

    infoNicknameVal->SetText(dash); infoOTVal->SetText(dash);
    infoPokerusVal->SetText(dash);  infoNatureVal->SetText(dash);
    infoAbilityVal->SetText(dash);  infoItemVal->SetText(dash);
    infoHpTypeVal->SetText(dash);

    statHpIV->SetText(dash);   statHpEV->SetText(dash);   statHpST->SetText(dash);
    statAtkIV->SetText(dash);  statAtkEV->SetText(dash);  statAtkST->SetText(dash);
    statDefIV->SetText(dash);  statDefEV->SetText(dash);  statDefST->SetText(dash);
    statSpaIV->SetText(dash);  statSpaEV->SetText(dash);  statSpaST->SetText(dash);
    statSpdIV->SetText(dash);  statSpdEV->SetText(dash);  statSpdST->SetText(dash);
    statSpeeIV->SetText(dash); statSpeeEV->SetText(dash); statSpeeST->SetText(dash);

    moveSlot1->SetText(dash); moveSlot2->SetText(dash);
    moveSlot3->SetText(dash); moveSlot4->SetText(dash);
}

// ─── Data ──────────────────────────────────────────────────────────────────────

void EditorScreen::LoadData() {
    struct SelectionSyncGuard {
        bool& flag;
        explicit SelectionSyncGuard(bool& v) : flag(v) { flag = true; }
        ~SelectionSyncGuard() { flag = false; }
    } guard(suppressPanelSelectionSync);

    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    if (!saveData || !boxDataProvider) {
        boxesPanel->SetBoxCount(1);
        pksm::ui::BoxData b("Box 1"); b.resize(30);
        boxesPanel->SetBoxData(0, b); boxesPanel->ForceRefreshCurrentBox();

        teamPanel->SetBoxCount(1);
        pksm::ui::BoxData t("Team"); t.resize(6);
        teamPanel->SetBoxData(0, t); teamPanel->ForceRefreshCurrentBox();
        ClearInfoPanel();
        return;
    }

    // Preserve current box position across reloads (e.g. after edit/delete)
    const int prevBox  = boxesPanel->GetCurrentBox();
    const int prevSlot = boxesPanel->GetSelectedSlot();

    const size_t boxCount = boxDataProvider->GetBoxCount(saveData);
    boxesPanel->SetBoxCount(boxCount);
    for (size_t i = 0; i < boxCount; i++) {
        auto box = boxDataProvider->GetBoxData(saveData, static_cast<int>(i));
        boxesPanel->SetBoxData(static_cast<int>(i), box);
    }
    // Restore to last position (clamped to valid range)
    const int restoredBox = std::min(prevBox, static_cast<int>(boxCount) - 1);
    boxesPanel->SetCurrentBox(restoredBox);

    teamPanel->SetBoxCount(1);
    pksm::ui::BoxData team("Team");
    if (partyDataProvider) {
        team = partyDataProvider->GetPartyData(saveData);
    } else {
        team.resize(6);
    }

    auto countTeamSlots = [&team]() {
        int count = 0;
        for (const auto& p : team.pokemon) {
            if (!p.isEmpty()) {
                count++;
            }
        }
        return count;
    };

    // Fallback: if provider returned an empty visual team, probe slot-by-slot.
    // This keeps Editor usable on saves where party bulk extraction fails.
    int teamCount = countTeamSlots();
    if (teamCount == 0 && partyDataProvider) {
        team.resize(6);
        for (int i = 0; i < 6; i++) {
            auto pk = partyDataProvider->GetPartyPokemon(saveData, i);
            if (!pk) {
                continue;
            }

            const u16 species = static_cast<u16>(pk->species());
            if (species == 0) {
                continue;
            }

            const u16 form_u16 = pk->alternativeForm();
            const u8 form = form_u16 > 255 ? 0 : static_cast<u8>(form_u16);
            team.pokemon[i] = pksm::ui::BoxPokemonData(species, form, pk->shiny());
        }
        teamCount = countTeamSlots();
        if (teamCount > 0) {
            LOG_WARNING("[EditorScreen] Party visual fallback populated " + std::to_string(teamCount) + " slot(s)");
        }
    }

    // If data is present but textures are missing, warn once to avoid confusion.
    static bool warnedMissingPartySprites = false;
    if (teamCount > 0 && !warnedMissingPartySprites) {
        int missingSpriteCount = 0;
        for (const auto& p : team.pokemon) {
            if (!p.isEmpty() && !p.getSprite()) {
                missingSpriteCount++;
            }
        }

        if (missingSpriteCount > 0) {
            warnedMissingPartySprites = true;
            LOG_WARNING(
                "[EditorScreen] Party sprites missing for " + std::to_string(missingSpriteCount) +
                " slot(s)"
            );
        }
    }

    LOG_DEBUG("[EditorScreen] LoadData: team Pokémon count=" + std::to_string(teamCount));
    teamPanel->SetBoxData(0, team);
    // Force the grid to update even if currentBox is already 0 by calling
    // UpdateBoxGrid directly (SetCurrentBox(0) is skipped by its guard).
    teamPanel->ForceRefreshCurrentBox();

    // Refresh the info panel for the currently selected slot
    if (activePanel == ActivePanel::Boxes) {
        const int curSlot = boxesPanel->GetSelectedSlot();
        auto pk = boxDataProvider->GetPokemon(saveData, restoredBox, curSlot);
        RefreshInfoPanel(pk ? pk.get() : nullptr);
    } else if (partyDataProvider) {
        auto pk = partyDataProvider->GetPartyPokemon(saveData, teamPanel->GetSelectedSlot());
        RefreshInfoPanel(pk ? pk.get() : nullptr);
    }
    (void)prevSlot;  // slot selection is preserved internally by PokemonBox
}

// ─── Context menu ──────────────────────────────────────────────────────────────

void EditorScreen::OpenActionMenu() {
    if (!actionOverlay) return;
    if (grab.active) {
        actionOverlay->SetTitle("Place Pok\xc3\xa9mon");
        actionOverlay->SetActions({"Place here", "Cancel move"});
    } else {
        actionOverlay->SetTitle("Actions");
        actionOverlay->SetActions({"Edit", "Move", "Clone", "Delete"});
    }
    actionOverlay->Reset();
    this->onShowOverlay(actionOverlay);
    actionOverlayVisible = true;
    // Disable both panels so their OnInput doesn't fire behind the overlay
    boxesPanel->SetDisabled(true);
    teamPanel->SetDisabled(true);
}

void EditorScreen::OpenPokemonEditOverlay() {
    if (pokemonEditOverlayVisible || actionOverlayVisible || inlineEditOpen) {
        return;
    }

    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    if (!saveData || !pokemonEditOverlay) {
        return;
    }

    editPanel = activePanel;
    editBox = boxesPanel->GetCurrentBox();
    editSlot = (activePanel == ActivePanel::Boxes) ? boxesPanel->GetSelectedSlot() : teamPanel->GetSelectedSlot();

    std::unique_ptr<pksm::PKX> pk;
    if (editPanel == ActivePanel::Boxes && boxDataProvider) {
        pk = boxDataProvider->GetPokemon(saveData, editBox, editSlot);
    } else if (editPanel == ActivePanel::Team && partyDataProvider) {
        pk = partyDataProvider->GetPartyPokemon(saveData, editSlot);
    }

    pokemonEditOverlay->SetPokemon(std::move(pk));
    this->onShowOverlay(pokemonEditOverlay);
    pokemonEditOverlayVisible = true;
    boxesPanel->SetDisabled(true);
    teamPanel->SetDisabled(true);
}

void EditorScreen::CommitPokemonEditOverlay() {
    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    if (!saveData || !pokemonEditOverlay) {
        ClosePokemonEditOverlay();
        return;
    }

    auto editedPk = pokemonEditOverlay->TakePokemon();
    if (!editedPk || static_cast<int>(editedPk->species()) == 0) {
        ClosePokemonEditOverlay();
        return;
    }

    editedPk->refreshChecksum();

    bool ok = false;
    if (editPanel == ActivePanel::Boxes && boxDataProvider) {
        ok = boxDataProvider->WritePokemon(saveData, editBox, editSlot, *editedPk);
    } else if (editPanel == ActivePanel::Team && partyDataProvider) {
        ok = partyDataProvider->WritePartyPokemon(saveData, editSlot, *editedPk);
    }

    ClosePokemonEditOverlay();
    LoadData();
    utils::NotificationManager::Push("Edit", ok ? "Pokemon Edited Succesfully" : "Pokemon Edit Failed");
}

void EditorScreen::ClosePokemonEditOverlay() {
    if (pokemonEditOverlay) {
        auto discard = pokemonEditOverlay->TakePokemon();
        (void)discard;
    }

    if (pokemonEditOverlayVisible) {
        this->onHideOverlay();
        pokemonEditOverlayVisible = false;
    }

    SetActivePanel(activePanel);
}

// Maps the 9 inline-edit fields to Y positions in the info panel.
// Fields: 0=Species(hdr), 1=Level(hdr), 2=Shiny(hdr),
//         3=Nature, 4=Ability, 5..8=Move1..Move4
void EditorScreen::UpdateEditHighlight() {
    if (!infoEditHighlight) return;  // safety
    // Row Y positions mirror the constructor layout.
    // Header occupies rows 0-2.
    const pu::i32 hdrY  = INFO_Y + (INFO_HDR_H - ROW_H) / 2;
    const pu::i32 row0Y = INFO_Y + INFO_HDR_H + 8;
    // row order in addRow calls: Nickname=0, OT=1, Pokérus=2, Nature=3, Ability=4, Item=5, HPType=6
    // Move rows start after stats section
    const pu::i32 natureY  = row0Y + 3 * ROW_H;          // 4th addRow (0-indexed 3)
    const pu::i32 abilityY = row0Y + 4 * ROW_H;          // 5th addRow
    // Stats section: 6 rows (header) + 6 stat rows
    const pu::i32 statsStartY = row0Y + 7 * ROW_H + 6;   // after 7 addRows + gap
    const pu::i32 movesStartY = statsStartY + (STAT_ROW_H - 2) + 6 * STAT_ROW_H + 6 + MOVES_HDR_H;

    pu::i32 hy = hdrY;
    pu::i32 hh = ROW_H;
    switch (editField) {
        case 0: case 1: case 2:
            hy = hdrY; hh = INFO_HDR_H; break;
        case 3:
            hy = natureY;  hh = ROW_H;      break;
        case 4:
            hy = abilityY; hh = ROW_H;      break;
        case 5:
            hy = movesStartY;                hh = MOVE_ROW_H; break;
        case 6:
            hy = movesStartY + MOVE_ROW_H;  hh = MOVE_ROW_H; break;
        case 7:
            hy = movesStartY + 2*MOVE_ROW_H; hh = MOVE_ROW_H; break;
        case 8:
            hy = movesStartY + 3*MOVE_ROW_H; hh = MOVE_ROW_H; break;
        default: break;
    }
    infoEditHighlight->SetY(hy);
    infoEditHighlight->SetHeight(hh);
}

void EditorScreen::OpenInlineEdit() {
    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    if (!saveData || !boxDataProvider || !partyDataProvider) return;

    editPanel = activePanel;
    editBox   = boxesPanel->GetCurrentBox();
    editSlot  = (activePanel == ActivePanel::Boxes)
                    ? boxesPanel->GetSelectedSlot()
                    : teamPanel->GetSelectedSlot();

    std::unique_ptr<pksm::PKX> pk = (editPanel == ActivePanel::Boxes)
        ? boxDataProvider->GetPokemon(saveData, editBox, editSlot)
        : partyDataProvider->GetPartyPokemon(saveData, editSlot);

    editSpecies = static_cast<int>(pk->species());
    editLevel   = std::max(1, std::min(100, (int)pk->level()));
    editShiny   = pk->shiny();
    editNature  = static_cast<int>(static_cast<u8>(pk->nature()));
    editAbility = static_cast<int>(static_cast<u16>(pk->ability()));
    editMove1   = static_cast<int>(static_cast<u16>(pk->move(0)));
    editMove2   = static_cast<int>(static_cast<u16>(pk->move(1)));
    editMove3   = static_cast<int>(static_cast<u16>(pk->move(2)));
    editMove4   = static_cast<int>(static_cast<u16>(pk->move(3)));
    editField   = 0;
    inlineEditOpen = true;
    // Disable panels so D-pad navigates fields, not box slots
    boxesPanel->SetDisabled(true);
    teamPanel->SetDisabled(true);
    // Show the field highlight and position it
    infoEditHighlight->SetVisible(true);
    UpdateEditHighlight();
}

void EditorScreen::HandleInlineEditInput(u64 down) {
    if (!inlineEditOpen) return;
    if (down & HidNpadButton_Up)   { editField = (editField + 8) % 9; UpdateEditHighlight(); }
    if (down & HidNpadButton_Down) { editField = (editField + 1) % 9; UpdateEditHighlight(); }

    auto dec = [&](int& v, int lo, int hi) { v = std::max(lo, v - 1); (void)hi; };
    auto inc = [&](int& v, int lo, int hi) { v = std::min(hi, v + 1); (void)lo; };

    if (down & HidNpadButton_Left) {
        if      (editField == 0) dec(editSpecies, 1, (int)pksm::PKX::PKSM_MAX_SPECIES);
        else if (editField == 1) dec(editLevel,   1, 100);
        else if (editField == 2) editShiny = !editShiny;
        else if (editField == 3) dec(editNature,  0, 24);
        else if (editField == 4) dec(editAbility, 0, 400);
        else if (editField == 5) dec(editMove1,   0, 1000);
        else if (editField == 6) dec(editMove2,   0, 1000);
        else if (editField == 7) dec(editMove3,   0, 1000);
        else if (editField == 8) dec(editMove4,   0, 1000);
    }
    if (down & HidNpadButton_Right) {
        if      (editField == 0) inc(editSpecies, 1, (int)pksm::PKX::PKSM_MAX_SPECIES);
        else if (editField == 1) inc(editLevel,   1, 100);
        else if (editField == 2) editShiny = !editShiny;
        else if (editField == 3) inc(editNature,  0, 24);
        else if (editField == 4) inc(editAbility, 0, 400);
        else if (editField == 5) inc(editMove1,   0, 1000);
        else if (editField == 6) inc(editMove2,   0, 1000);
        else if (editField == 7) inc(editMove3,   0, 1000);
        else if (editField == 8) inc(editMove4,   0, 1000);
    }

    EnsureI18n();
    // Reflect the live edit state into info-panel labels for immediate feedback
    infoLevelText->SetText("Lv." + std::to_string(editLevel));
    infoNatureVal->SetText(SafeStr(i18n::nature(pksm::Language::ENG, static_cast<pksm::Nature>(editNature))));
    infoAbilityVal->SetText(SafeStr(i18n::ability(pksm::Language::ENG, static_cast<pksm::Ability>(editAbility))));
    const std::string shinyMark = editShiny ? " \xe2\x98\x85" : "";
    infoSpeciesText->SetText(SafeStr(i18n::species(pksm::Language::ENG, static_cast<pksm::Species>(editSpecies))) + shinyMark);
    moveSlot1->SetText("\xc2\xb7 " + SafeStr(i18n::move(pksm::Language::ENG, static_cast<pksm::Move>(editMove1))));
    moveSlot2->SetText("\xc2\xb7 " + SafeStr(i18n::move(pksm::Language::ENG, static_cast<pksm::Move>(editMove2))));
    moveSlot3->SetText("\xc2\xb7 " + SafeStr(i18n::move(pksm::Language::ENG, static_cast<pksm::Move>(editMove3))));
    moveSlot4->SetText("\xc2\xb7 " + SafeStr(i18n::move(pksm::Language::ENG, static_cast<pksm::Move>(editMove4))));
}

void EditorScreen::CommitInlineEdit() {
    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    if (!saveData || !boxDataProvider || !partyDataProvider) return;

    std::unique_ptr<pksm::PKX> pk = (editPanel == ActivePanel::Boxes)
        ? boxDataProvider->GetPokemon(saveData, editBox, editSlot)
        : partyDataProvider->GetPartyPokemon(saveData, editSlot);
    if (!pk || static_cast<int>(pk->species()) == 0) {
        inlineEditOpen = false;
        infoEditHighlight->SetVisible(false);
        SetActivePanel(activePanel);
        return;
    }

    pk->species(static_cast<pksm::Species>(editSpecies));
    pk->level(static_cast<u8>(editLevel));
    pk->shiny(editShiny);
    pk->nature(static_cast<pksm::Nature>(editNature));
    pk->ability(static_cast<pksm::Ability>(editAbility));
    pk->move(0, static_cast<pksm::Move>(editMove1));
    pk->move(1, static_cast<pksm::Move>(editMove2));
    pk->move(2, static_cast<pksm::Move>(editMove3));
    pk->move(3, static_cast<pksm::Move>(editMove4));
    pk->refreshChecksum();

    bool ok = (editPanel == ActivePanel::Boxes)
        ? boxDataProvider->WritePokemon(saveData, editBox, editSlot, *pk)
        : partyDataProvider->WritePartyPokemon(saveData, editSlot, *pk);

    inlineEditOpen = false;
    infoEditHighlight->SetVisible(false);
    SetActivePanel(activePanel);
    LoadData();
        utils::NotificationManager::Push("Edit", ok ? "Pokemon Edited Succesfully" : "Pokemon Edit Failed");
}

void EditorScreen::ExecuteContextAction(ContextAction action) {
    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    if (!boxDataProvider || !partyDataProvider) {
        return;
    }
    if (!saveData) {
        return;
    }

    const bool onBoxes = (activePanel == ActivePanel::Boxes);
    const int slot = onBoxes ? boxesPanel->GetSelectedSlot() : teamPanel->GetSelectedSlot();
    const int box  = boxesPanel->GetCurrentBox();

    if (action == ContextAction::Edit) { OpenPokemonEditOverlay(); return; }

    if (action == ContextAction::Delete) {
        bool ok = onBoxes
            ? boxDataProvider->ClearSlot(saveData, box, slot)
            : partyDataProvider->ClearPartySlot(saveData, slot);
        LoadData();
        return;
    }

    if (action == ContextAction::Clone) {
        std::unique_ptr<pksm::PKX> src = onBoxes
            ? boxDataProvider->GetPokemon(saveData, box, slot)
            : partyDataProvider->GetPartyPokemon(saveData, slot);
        const int next = std::min(slot + 1, onBoxes ? 29 : 5);
        bool ok = onBoxes
            ? boxDataProvider->WritePokemon(saveData, box, next, *src)
            : partyDataProvider->WritePartyPokemon(saveData, next, *src);
        LoadData();
        return;
    }

    // Move (grab/drop)
    if (!grab.active) {
        grab = { true, activePanel, box, slot };
        titleText->SetText("Editor  \xe2\x9c\xa6 MOVING");  // visual grab indicator
        return;
    }
    if (grab.panel == activePanel && grab.slot == slot && (!onBoxes || grab.box == box)) {
        grab.active = false;
        titleText->SetText("Editor");
        return;
    }

    std::unique_ptr<pksm::PKX> src = (grab.panel == ActivePanel::Boxes)
        ? boxDataProvider->GetPokemon(saveData, grab.box, grab.slot)
        : partyDataProvider->GetPartyPokemon(saveData, grab.slot);
    std::unique_ptr<pksm::PKX> dst = onBoxes
        ? boxDataProvider->GetPokemon(saveData, box, slot)
        : partyDataProvider->GetPartyPokemon(saveData, slot);

    bool ok = true;
    if (src && static_cast<int>(src->species()) != 0) {
        if (onBoxes) ok &= boxDataProvider->WritePokemon(saveData, box, slot, *src);
        else         ok &= partyDataProvider->WritePartyPokemon(saveData, slot, *src);
    } else {
        if (onBoxes) ok &= boxDataProvider->ClearSlot(saveData, box, slot);
        else         ok &= partyDataProvider->ClearPartySlot(saveData, slot);
    }
    if (dst && static_cast<int>(dst->species()) != 0) {
        if (grab.panel == ActivePanel::Boxes)
            ok &= boxDataProvider->WritePokemon(saveData, grab.box, grab.slot, *dst);
        else
            ok &= partyDataProvider->WritePartyPokemon(saveData, grab.slot, *dst);
    } else {
        if (grab.panel == ActivePanel::Boxes)
            ok &= boxDataProvider->ClearSlot(saveData, grab.box, grab.slot);
        else
            ok &= partyDataProvider->ClearPartySlot(saveData, grab.slot);
    }
    grab.active = false;
    titleText->SetText("Editor");
    LoadData();
}

// ─── Primary action ────────────────────────────────────────────────────────────

void EditorScreen::HandlePrimaryAction() {
    // If action overlay is open, confirm the selection
    if (actionOverlayVisible) {
        const std::string& act = actionOverlay->GetSelectedAction();
        ContextAction ca = ContextAction::Edit;
        if      (act == "Move" || act == "Place here") ca = ContextAction::Move;
        else if (act == "Clone")       ca = ContextAction::Clone;
        else if (act == "Delete")      ca = ContextAction::Delete;
        else if (act == "Cancel move") {
            // Cancel the grab without doing anything
            this->onHideOverlay();
            actionOverlayVisible = false;
            SetActivePanel(activePanel);
            grab.active = false;
            return;
        }

        this->onHideOverlay();
        actionOverlayVisible = false;
        // Restore panel input state before executing the action.
        SetActivePanel(activePanel);
        ExecuteContextAction(ca);
        return;
    }
    OpenActionMenu();
}

// ─── Panel switching ───────────────────────────────────────────────────────────

void EditorScreen::SetActivePanel(ActivePanel panel) {
    // Don't switch panels while an overlay or inline editor is open
    if (actionOverlayVisible || inlineEditOpen || pokemonEditOverlayVisible) return;

    activePanel = panel;
    const bool boxes = (activePanel == ActivePanel::Boxes);
    boxesPanel->SetSelected(boxes);  boxesPanel->SetFocused(boxes);
    teamPanel->SetSelected(!boxes);  teamPanel->SetFocused(!boxes);
    // Keep input exclusive to the active panel to prevent hidden cross-panel interactions.
    boxesPanel->SetDisabled(!boxes);
    teamPanel->SetDisabled(boxes);

    auto saveData = saveDataAccessor ? saveDataAccessor->getCurrentSaveData() : nullptr;
    if (saveData) {
        if (boxes && boxDataProvider) {
            auto pk = boxDataProvider->GetPokemon(saveData, boxesPanel->GetCurrentBox(),
                                                   boxesPanel->GetSelectedSlot());
            RefreshInfoPanel(pk ? pk.get() : nullptr);
        } else if (!boxes && partyDataProvider) {
            auto pk = partyDataProvider->GetPartyPokemon(saveData, teamPanel->GetSelectedSlot());
            RefreshInfoPanel(pk ? pk.get() : nullptr);
        }
    }
}

// ─── Input ─────────────────────────────────────────────────────────────────────

void EditorScreen::OnInput(u64 down, u64 up, u64 held) {
    if (HandleHelpInput(down)) return;

    // Dedicated edit screen overlay input
    if (pokemonEditOverlayVisible) {
        if (down & HidNpadButton_B) {
            ClosePokemonEditOverlay();
            utils::NotificationManager::Push("Edit", "Cancelled Pokemon Edit");
            return;
        }
        if (down & HidNpadButton_Up) {
            pokemonEditOverlay->MoveUp();
            return;
        }
        if (down & HidNpadButton_Down) {
            pokemonEditOverlay->MoveDown();
            return;
        }
        if (down & HidNpadButton_Left) {
            pokemonEditOverlay->MoveLeft();
            return;
        }
        if (down & HidNpadButton_Right) {
            pokemonEditOverlay->MoveRight();
            return;
        }
        if (down & HidNpadButton_A) {
            const auto result = pokemonEditOverlay->Confirm();
            if (result == pksm::ui::PokemonEditOverlay::Result::Save) {
                CommitPokemonEditOverlay();
            } else if (result == pksm::ui::PokemonEditOverlay::Result::Cancel) {
                ClosePokemonEditOverlay();
            }
            return;
        }
        return;
    }

    // Inline field editor
    if (inlineEditOpen) {
        if (down & HidNpadButton_B) {
            inlineEditOpen = false;
            infoEditHighlight->SetVisible(false);
            SetActivePanel(activePanel);
            return;
        }
        if (down & HidNpadButton_A) { CommitInlineEdit(); return; }
        HandleInlineEditInput(down);
        return;
    }

    // Action overlay navigation
    if (actionOverlayVisible) {
        if (down & HidNpadButton_B) {
            this->onHideOverlay();
            actionOverlayVisible = false;
            SetActivePanel(activePanel);
            return;
        }
        if (down & HidNpadButton_Up)   { actionOverlay->MoveUp();   return; }
        if (down & HidNpadButton_Down) { actionOverlay->MoveDown(); return; }
        if (down & HidNpadButton_A)    { HandlePrimaryAction();      return; }
        return;
    }

    buttonHandler.HandleInput(down, up, held);
}

// ─── Help overlay ──────────────────────────────────────────────────────────────

std::vector<pksm::ui::HelpItem> EditorScreen::GetHelpOverlayItems() const {
    return {
        {{{pksm::ui::global::ButtonGlyph::DPad}},                                   "Navigate slots"},
        {{{pksm::ui::global::ButtonGlyph::A}},                                      "Open action menu"},
        {{{pksm::ui::global::ButtonGlyph::X}},                                      "Switch Box/Team"},
        {{{pksm::ui::global::ButtonGlyph::L}, {pksm::ui::global::ButtonGlyph::R}},  "Switch Box"},
        {{{pksm::ui::global::ButtonGlyph::B}},                                      "Back to Main Menu"}
    };
}

} // namespace pksm::layout
