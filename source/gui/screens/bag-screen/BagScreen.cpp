#include "gui/screens/bag-screen/BagScreen.hpp"
#include "gui/shared/UIConstants.hpp"
#include "gui/shared/components/AnimatedBackground.hpp"
#include "gui/shared/interfaces/IHelpProvider.hpp"
#include "utils/Logger.hpp"

#include "data/saves/SaveData.hpp"
#include "pksmcore/enums/Language.hpp"
#include "pksmcore/utils/i18n.hpp"
#include "pksmcore/utils/VersionTables.hpp"
#include <set>

namespace pksm::layout {

BagScreen::BagScreen(
    ISaveDataAccessor::Ref saveDataAccessor,
    std::function<void()> onBack,
    std::function<void(pu::ui::Overlay::Ref)> onShowOverlay,
    std::function<void()> onHideOverlay
)
  : BaseLayout(onShowOverlay, onHideOverlay),
    saveDataAccessor(std::move(saveDataAccessor)),
    onBack(onBack),
    buttonHandler(),
    isAddItemOverlayVisible(false),
    addItemOverlay(nullptr),
    addItemList(nullptr),
    addItemIds() {
    
    LOG_DEBUG("Initializing BagScreen...");

    this->SetBackgroundColor(bgColor);
    background = ui::AnimatedBackground::New();
    this->Add(background);

    headerText = pu::ui::elm::TextBlock::New(SIDE_MARGIN, HEADER_TOP_MARGIN, "Bag");
    headerText->SetColor(pksm::ui::global::TEXT_WHITE);
    headerText->SetFont(pksm::ui::global::MakeHeavyFontName(pksm::ui::global::FONT_SIZE_TITLE));
    this->Add(headerText);

    bagScreenFocusManager = pksm::input::FocusManager::New("BagScreen Manager");
    bagScreenFocusManager->SetActive(true);

    CreateCategoryButtons();

    CreateItemControls();

    for (auto& button : categoryButtons) {
        bagScreenFocusManager->RegisterFocusable(button);
    }

    if (!categoryButtons.empty()) {
        categoryButtons[0]->RequestFocus();
    }

    InitializeHelpFooter();

    directionalHandler.SetOnMoveDown([this]() {
        auto currentFocused = bagScreenFocusManager ? bagScreenFocusManager->GetFocusedElement() : nullptr;
        if (!currentFocused) {
            return;
        }

        if (currentCategory == -1) {
            for (size_t i = 0; i < categoryButtons.size(); i++) {
                if (categoryButtons[i] == currentFocused && i + 1 < categoryButtons.size()) {
                    categoryButtons[i + 1]->RequestFocus();
                    break;
                }
            }
        } else {
            if (currentFocused == decreaseButton) {
                increaseButton->RequestFocus();
            } else if (currentFocused == increaseButton) {
                backButton->RequestFocus();
            }
        }
    });

    directionalHandler.SetOnMoveUp([this]() {
        auto currentFocused = bagScreenFocusManager ? bagScreenFocusManager->GetFocusedElement() : nullptr;
        if (!currentFocused) {
            return;
        }

        if (currentCategory == -1) {
            for (size_t i = 0; i < categoryButtons.size(); i++) {
                if (categoryButtons[i] == currentFocused && i > 0) {
                    categoryButtons[i - 1]->RequestFocus();
                    break;
                }
            }
        } else {
            if (currentFocused == backButton) {
                increaseButton->RequestFocus();
            } else if (currentFocused == increaseButton) {
                decreaseButton->RequestFocus();
            } else if (currentFocused == decreaseButton) {
                if (itemList) {
                    itemList->SetFocused(true);
                    itemList->RequestFocus();
                }
            }
        }
    });

    directionalHandler.SetOnMoveLeft([this]() {
        if (currentCategory == -1) {
            return;
        }

        auto currentFocused = bagScreenFocusManager ? bagScreenFocusManager->GetFocusedElement() : nullptr;
        if (!currentFocused) {
            return;
        }

        if (currentFocused == increaseButton) {
            decreaseButton->RequestFocus();
        } else if (currentFocused == backButton) {
            increaseButton->RequestFocus();
        }
    });

    directionalHandler.SetOnMoveRight([this]() {
        if (currentCategory == -1) {
            return;
        }

        auto currentFocused = bagScreenFocusManager ? bagScreenFocusManager->GetFocusedElement() : nullptr;
        if (!currentFocused) {
            return;
        }

        if (currentFocused == decreaseButton) {
            increaseButton->RequestFocus();
        } else if (currentFocused == increaseButton) {
            backButton->RequestFocus();
        }
    });

    this->SetOnInput(
        std::bind(&BagScreen::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );

    LOG_DEBUG("BagScreen initialization complete");
    PreRender();
}

BagScreen::~BagScreen() {
    LOG_DEBUG("BagScreen destructor");
}

void BagScreen::RefreshCategories() {
    BuildCategoriesForCurrentSave();

    const size_t visible_count = categoryLabels.size();
    for (size_t i = 0; i < categoryButtons.size(); i++) {
        if (i < visible_count) {
            categoryButtons[i]->SetContent(categoryLabels[i]);
            categoryButtons[i]->SetHelpText("Open " + categoryLabels[i] + " category");
            categoryButtons[i]->SetVisible(true);
        } else {
            categoryButtons[i]->SetVisible(false);
        }
    }

    if (currentCategory == -1) {
        for (auto &btn : categoryButtons) {
            if (btn->IsVisible()) {
                btn->RequestFocus();
                break;
            }
        }
    }
}

void BagScreen::OnInput(u64 down, u64 up, u64 held) {
    if (HandleHelpInput(down)) {
        return;
    }

    if (isAddItemOverlayVisible) {
        if (!addItemList) {
            HideAddItemOverlay();
            return;
        }

        if (down & HidNpadButton_B) {
            HideAddItemOverlay();
            return;
        }

        if (down & HidNpadButton_A) {
            ConfirmAddSelectedItem();
            return;
        }

        addItemList->OnInput(down, up, held, pu::ui::TouchPoint());
        return;
    }

    if (down & HidNpadButton_B) {
        if (currentCategory == -1) {
            LOG_DEBUG("B button pressed, returning to main menu");
            if (this->onBack) {
                this->onBack();
            }
        } else {
            ShowCategory(-1);
        }
        return;
    }

    if ((currentCategory != -1) && (down & HidNpadButton_X)) {
        ShowAddItemOverlay();
        return;
    }

    buttonHandler.HandleInput(down, up, held);

    if ((currentCategory != -1) && itemList && itemList->IsFocused()) {
        itemList->OnInput(down, up, held, pu::ui::TouchPoint());
        UpdateItemDisplay();
        return;
    }

    directionalHandler.HandleInput(down, held);

    auto focused = bagScreenFocusManager->GetFocusedElement();
    if (focused && (down & HidNpadButton_A)) {
        if (currentCategory == -1) {
            for (auto& button : categoryButtons) {
                if (button == focused) {
                    button->OnInput(down, up, held, pu::ui::TouchPoint());
                    break;
                }
            }
        } else {
            if (focused == decreaseButton) {
                decreaseButton->OnInput(down, up, held, pu::ui::TouchPoint());
            } else if (focused == increaseButton) {
                increaseButton->OnInput(down, up, held, pu::ui::TouchPoint());
            } else if (focused == backButton) {
                backButton->OnInput(down, up, held, pu::ui::TouchPoint());
            }
        }
    }
}

std::vector<pksm::ui::HelpItem> BagScreen::GetHelpOverlayItems() const {
    return {
        {{{pksm::ui::global::ButtonGlyph::A}}, "Select"},
        {{{pksm::ui::global::ButtonGlyph::B}}, "Back to Main Menu"},
        {{{pksm::ui::global::ButtonGlyph::DPad}}, "Navigate"},
        {{{pksm::ui::global::ButtonGlyph::Minus}}, "Help"}
    };
}

void BagScreen::OnHelpOverlayShown() {
    LOG_DEBUG("Help overlay shown, disabling UI elements");
    if (currentCategory == -1) {
        for (auto& button : categoryButtons) {
            button->SetDisabled(true);
        }
    } else {
        decreaseButton->SetDisabled(true);
        increaseButton->SetDisabled(true);
        backButton->SetDisabled(true);
    }
}

void BagScreen::OnHelpOverlayHidden() {
    LOG_DEBUG("Help overlay hidden, re-enabling UI elements");
    if (currentCategory == -1) {
        for (auto& button : categoryButtons) {
            button->SetDisabled(false);
        }
    } else {
        decreaseButton->SetDisabled(false);
        increaseButton->SetDisabled(false);
        backButton->SetDisabled(false);
    }
}

void BagScreen::BuildCategoriesForCurrentSave() {
    categoryLabels.clear();
    categoryPouches.clear();

    auto version = pksm::saves::GameVersion::RD;
    if (this->saveDataAccessor) {
        auto saveData = this->saveDataAccessor->getCurrentSaveData();
        if (saveData) {
            version = saveData->getVersion();
        }
    }

    const bool is_za = (version == pksm::saves::GameVersion::ZA);
    const bool is_sv = (version == pksm::saves::GameVersion::SL) || (version == pksm::saves::GameVersion::VL);
    const bool is_pla = (version == pksm::saves::GameVersion::PLA);
    const bool is_swsh = (version == pksm::saves::GameVersion::SW) || (version == pksm::saves::GameVersion::SH);
    const bool is_lgpe = (version == pksm::saves::GameVersion::GP) || (version == pksm::saves::GameVersion::GE);
    const bool is_gsc = (version == pksm::saves::GameVersion::GD) || (version == pksm::saves::GameVersion::SV) || (version == pksm::saves::GameVersion::C);
    const bool is_rgby = (version == pksm::saves::GameVersion::RD) || (version == pksm::saves::GameVersion::GN) || (version == pksm::saves::GameVersion::BU) || (version == pksm::saves::GameVersion::YW);

    if (is_za) {
        categoryLabels = {
            "Medicine",
            "Poke Balls",
            "Berries",
            "Items",
            "TMs",
            "Mega Stones",
            "Treasure",
            "Key Items",
        };

        categoryPouches = {
            pksm::saves::BagPouch::Medicine,
            pksm::saves::BagPouch::Ball,
            pksm::saves::BagPouch::Berry,
            pksm::saves::BagPouch::NormalItem,
            pksm::saves::BagPouch::TM,
            pksm::saves::BagPouch::MegaStones,
            pksm::saves::BagPouch::Treasure,
            pksm::saves::BagPouch::KeyItem,
        };
    } else if (is_sv) {
        categoryLabels = {
            "Items",
            "Key Items",
            "PC Items",
        };

        categoryPouches = {
            pksm::saves::BagPouch::NormalItem,
            pksm::saves::BagPouch::KeyItem,
            pksm::saves::BagPouch::PCItem,
        };
    } else if (is_pla) {
        categoryLabels = {
            "Items",
            "Key Items",
            "PC Items",
        };

        categoryPouches = {
            pksm::saves::BagPouch::NormalItem,
            pksm::saves::BagPouch::KeyItem,
            pksm::saves::BagPouch::PCItem,
        };
    } else if (is_swsh) {
        categoryLabels = {
            "Medicine",
            "Poke Balls",
            "Battle Items",
            "Berries",
            "Other Items",
            "TMs",
            "Treasures",
            "Ingredients",
            "Key Items",
        };

        categoryPouches = {
            pksm::saves::BagPouch::Medicine,
            pksm::saves::BagPouch::Ball,
            pksm::saves::BagPouch::Battle,
            pksm::saves::BagPouch::Berry,
            pksm::saves::BagPouch::NormalItem,
            pksm::saves::BagPouch::TM,
            pksm::saves::BagPouch::Treasure,
            pksm::saves::BagPouch::Ingredient,
            pksm::saves::BagPouch::KeyItem,
        };
    } else if (is_lgpe) {
        categoryLabels = {
            "Medicine",
            "TMs/HMs",
            "Candies",
            "Power-Up Items",
            "Catching Items",
            "Battle Items",
            "Items",
        };

        categoryPouches = {
            pksm::saves::BagPouch::Medicine,
            pksm::saves::BagPouch::TM,
            pksm::saves::BagPouch::Candy,
            pksm::saves::BagPouch::ZCrystals,
            pksm::saves::BagPouch::CatchingItem,
            pksm::saves::BagPouch::Battle,
            pksm::saves::BagPouch::NormalItem,
        };
    } else if (is_gsc) {
        categoryLabels = {
            "TMs/HMs",
            "Items",
            "Key Items",
            "Balls",
            "PC Items",
        };

        categoryPouches = {
            pksm::saves::BagPouch::TM,
            pksm::saves::BagPouch::NormalItem,
            pksm::saves::BagPouch::KeyItem,
            pksm::saves::BagPouch::Ball,
            pksm::saves::BagPouch::PCItem,
        };
    } else if (is_rgby) {
        categoryLabels = {
            "Items",
            "PC Items",
        };

        categoryPouches = {
            pksm::saves::BagPouch::NormalItem,
            pksm::saves::BagPouch::PCItem,  
        };
    } else {
        categoryLabels = {
            "Items",
        };
 
        categoryPouches = {
            pksm::saves::BagPouch::NormalItem,
        };
    }
}
 
void BagScreen::CreateCategoryButtons() {
    BuildCategoriesForCurrentSave();
 
    pu::i32 currentY = CATEGORY_TOP_MARGIN;
    for (size_t i = 0; i < MAX_CATEGORY_BUTTONS; i++) {
        auto button = pksm::ui::FocusableButton::New(
            SIDE_MARGIN,
            currentY,
            BUTTON_WIDTH,
            BUTTON_HEIGHT,
            "",
            pu::ui::Color(140, 110, 0, 200),
            pu::ui::Color(200, 160, 0, 255)
        );
 
        button->SetContentFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_BUTTON));
        button->SetContentColor(pksm::ui::global::TEXT_WHITE);
        button->SetOnClick([this, idx = i]() {
            LOG_DEBUG("Category " + std::to_string(idx) + " clicked");
            ShowCategory(static_cast<int>(idx));
        });
 
        this->Add(button);
        categoryButtons.push_back(button);
        currentY += BUTTON_HEIGHT + BUTTON_SPACING;
    }
 
    RefreshCategories();
}
 
void BagScreen::CreateItemControls() {
    categoryHeaderText = pu::ui::elm::TextBlock::New(
        SIDE_MARGIN, 
        CATEGORY_TOP_MARGIN, 
        ""
    );
    categoryHeaderText->SetColor(pksm::ui::global::TEXT_WHITE);
    categoryHeaderText->SetFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_HEADER));
    categoryHeaderText->SetVisible(false);
    this->Add(categoryHeaderText);
 
    itemNameText = pu::ui::elm::TextBlock::New(
        SIDE_MARGIN,
        ITEM_CONTROL_TOP_MARGIN,
        "Sample Item"
    );
    itemNameText->SetColor(pksm::ui::global::TEXT_WHITE);
    itemNameText->SetFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_BUTTON));
    itemNameText->SetVisible(false);
    this->Add(itemNameText);
 
    itemList = pksm::ui::FocusableMenu::New(
        SIDE_MARGIN,
        ITEM_CONTROL_TOP_MARGIN,
        700,
        pu::ui::Color(140, 110, 0, 200),
        pu::ui::Color(200, 160, 0, 255),
        70,
        5
    );
    itemList->SetVisible(false);
    itemList->SetOnSelectionChanged([this]() { this->UpdateItemDisplay(); });
    itemList->SetOnCancel([this]() { ShowCategory(-1); });
    itemList->SetOnFocusExit([this](pksm::ui::FocusExitDirection dir) {
        if (dir != pksm::ui::FocusExitDirection::Down) {
            return;
        }
        if (itemList) {
            itemList->SetFocused(false);
        }
        if (decreaseButton) {
            decreaseButton->RequestFocus();
        }
    });
    bagScreenFocusManager->RegisterFocusable(itemList);
    this->Add(itemList);
 
    decreaseButton = pksm::ui::FocusableButton::New(
        SIDE_MARGIN,
        ITEM_CONTROL_TOP_MARGIN + 400,
        100,
        BUTTON_HEIGHT,
        "-",
        pu::ui::Color(140, 110, 0, 200),
        pu::ui::Color(200, 160, 0, 255)
    );
    decreaseButton->SetContentFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_BUTTON));
    decreaseButton->SetContentColor(pksm::ui::global::TEXT_WHITE);
    decreaseButton->SetOnClick([this]() {
        if (currentItemQuantity > 0) {
            currentItemQuantity--;
            auto saveData = this->saveDataAccessor ? this->saveDataAccessor->getCurrentSaveData() : nullptr;
            if (saveData && itemList && !currentItemMap.empty()) {
                const auto sel = itemList->GetSelectedIndex();
                if ((sel >= 0) && (static_cast<size_t>(sel) < currentItemMap.size())) {
                    const auto bag_idx = currentItemMap.at(static_cast<size_t>(sel));
                    const auto &items = saveData->getBagItems();
                    if (bag_idx < items.size()) {
                        const auto &item = items.at(bag_idx);
                        const auto itemName = i18n::item(pksm::Language::ENG, item.itemId);
                        const bool isMegaStone = !itemName.empty() && itemName.find("NAITO") != std::string::npos;
                        const u16 maxQty = (saveData->getVersion() == pksm::saves::GameVersion::ZA && isMegaStone) ? 1 : 999;
                        const u16 newQty = std::min(static_cast<u16>(currentItemQuantity), maxQty);
                        if (newQty > 0) {
                            auto updated = items;
                            updated.at(bag_idx).count = newQty;
                            saveData->setBagItems(std::move(updated));
                            this->saveDataAccessor->markUnsavedChanges();
                            currentItemQuantity = newQty;
                        }
                    }
                }
            }
            UpdateItemDisplay();
        }
    });
    decreaseButton->SetVisible(false);
    this->Add(decreaseButton);
 
    itemQuantityText = pu::ui::elm::TextBlock::New(
        SIDE_MARGIN + 120,
        ITEM_CONTROL_TOP_MARGIN + 425,
        "x10"
    );
    itemQuantityText->SetColor(pksm::ui::global::TEXT_WHITE);
    itemQuantityText->SetFont(pksm::ui::global::MakeHeavyFontName(pksm::ui::global::FONT_SIZE_BUTTON));
    itemQuantityText->SetVisible(false);
    this->Add(itemQuantityText);
 
    increaseButton = pksm::ui::FocusableButton::New(
        SIDE_MARGIN + 340,
        ITEM_CONTROL_TOP_MARGIN + 400,
        100,
        BUTTON_HEIGHT,
        "+",
        pu::ui::Color(140, 110, 0, 200),
        pu::ui::Color(200, 160, 0, 255)
    );
    increaseButton->SetContentFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_BUTTON));
    increaseButton->SetContentColor(pksm::ui::global::TEXT_WHITE);
    increaseButton->SetOnClick([this]() {
        currentItemQuantity++;
        auto saveData = this->saveDataAccessor ? this->saveDataAccessor->getCurrentSaveData() : nullptr;
        if (saveData && itemList && !currentItemMap.empty()) {
            const auto sel = itemList->GetSelectedIndex();
            if ((sel >= 0) && (static_cast<size_t>(sel) < currentItemMap.size())) {
                const auto bag_idx = currentItemMap.at(static_cast<size_t>(sel));
                const auto &items = saveData->getBagItems();
                if (bag_idx < items.size()) {
                    const auto &item = items.at(bag_idx);
                    const auto itemName = i18n::item(pksm::Language::ENG, item.itemId);
                    const bool isMegaStone = !itemName.empty() && itemName.find("NAITO") != std::string::npos;
                    const u16 maxQty = (saveData->getVersion() == pksm::saves::GameVersion::ZA && isMegaStone) ? 1 : 999;
                    const u16 newQty = std::min(static_cast<u16>(currentItemQuantity), maxQty);
                    if (newQty > 0) {
                        auto updated = items;
                        updated.at(bag_idx).count = newQty;
                        saveData->setBagItems(std::move(updated));
                        this->saveDataAccessor->markUnsavedChanges();
                        currentItemQuantity = newQty;
                    }
                }
            }
        }
        UpdateItemDisplay();
    });
    increaseButton->SetVisible(false);
    this->Add(increaseButton);

    backButton = pksm::ui::FocusableButton::New(
        SIDE_MARGIN,
        ITEM_CONTROL_TOP_MARGIN + 500,
        BUTTON_WIDTH,
        BUTTON_HEIGHT,
        "Back to Categories",
        pu::ui::Color(140, 110, 0, 200),
        pu::ui::Color(200, 160, 0, 255)
    );
    backButton->SetContentFont(pksm::ui::global::MakeMediumFontName(pksm::ui::global::FONT_SIZE_BUTTON));
    backButton->SetContentColor(pksm::ui::global::TEXT_WHITE);
    backButton->SetOnClick([this]() {
        ShowCategory(-1);
    });
    backButton->SetVisible(false);
    this->Add(backButton);
 
    bagScreenFocusManager->RegisterFocusable(decreaseButton);
    bagScreenFocusManager->RegisterFocusable(increaseButton);
    bagScreenFocusManager->RegisterFocusable(backButton);
}
 
void BagScreen::ShowAddItemOverlay() {
    if (isAddItemOverlayVisible) {
        return;
    }
 
    addItemOverlay = pu::ui::Overlay::New(0, 0, GetWidth(), GetHeight(), pu::ui::Color(0, 0, 0, 200));
    addItemOverlay->SetRadius(0);
 
    auto title = pu::ui::elm::TextBlock::New(SIDE_MARGIN, HEADER_TOP_MARGIN, "Add Item");
    title->SetColor(pksm::ui::global::TEXT_WHITE);
    title->SetFont(pksm::ui::global::MakeHeavyFontName(pksm::ui::global::FONT_SIZE_TITLE));
    addItemOverlay->Add(title);
 
    addItemList = pksm::ui::FocusableMenu::New(
        SIDE_MARGIN,
        CATEGORY_TOP_MARGIN,
        1040,
        pu::ui::Color(40, 40, 40, 220),
        pu::ui::Color(80, 80, 80, 255),
        70,
        8
    );
    addItemList->SetFocused(true);
    addItemList->SetOnCancel([this]() { this->HideAddItemOverlay(); });
    addItemOverlay->Add(addItemList);
 
    RefreshAddItemListForCurrentCategory();
 
    onShowOverlay(addItemOverlay);
    isAddItemOverlayVisible = true;
}
 
void BagScreen::HideAddItemOverlay() {
    if (!isAddItemOverlayVisible) {
        return;
    }
 
    onHideOverlay();
    isAddItemOverlayVisible = false;
    addItemOverlay = nullptr;
    addItemList = nullptr;
    addItemIds.clear();
 
    if (itemList && (currentCategory != -1)) {
        itemList->SetFocused(true);
        itemList->RequestFocus();
    }
}
 
void BagScreen::RefreshAddItemListForCurrentCategory() {
    addItemIds.clear();
    if (!addItemList) {
        return;
    }

    if (!saveDataAccessor) {
        addItemList->SetDataSource({"No save loaded"});
        return;
    }

    auto saveData = saveDataAccessor->getCurrentSaveData();
    if (!saveData) {
        addItemList->SetDataSource({"No save loaded"});
        return;
    }

    pksm::saves::BagPouch pouch = pksm::saves::BagPouch::Unknown;
    if ((currentCategory >= 0) && (static_cast<size_t>(currentCategory) < categoryPouches.size())) {
        pouch = categoryPouches.at(static_cast<size_t>(currentCategory));
    }

    std::set<u16> existing;
    for (const auto &it : saveData->getBagItems()) {
        if (it.pouch != pouch) {
            continue;
        }
        existing.insert(it.itemId);
    }

    std::vector<std::string> names;
    const auto toCoreVersion = [](pksm::saves::GameVersion v) -> pksm::GameVersion {
        using GV = pksm::GameVersion;
        using AGV = pksm::saves::GameVersion;

        switch (v) {
            case AGV::RD:
                return GV::RD;
            case AGV::GN:
                return GV::GN;
            case AGV::BU:
                return GV::BU;
            case AGV::YW:
                return GV::YW;
            case AGV::GD:
                return GV::GD;
            case AGV::SV:
                return GV::SV;
            case AGV::C:
                return GV::C;
            case AGV::R:
                return GV::R;
            case AGV::S:
                return GV::S;
            case AGV::E:
                return GV::E;
            case AGV::FR:
                return GV::FR;
            case AGV::LG:
                return GV::LG;
            case AGV::D:
                return GV::D;
            case AGV::P:
                return GV::P;
            case AGV::Pt:
                return GV::Pt;
            case AGV::HG:
                return GV::HG;
            case AGV::SS:
                return GV::SS;
            case AGV::W:
                return GV::W;
            case AGV::B:
                return GV::B;
            case AGV::W2:
                return GV::W2;
            case AGV::B2:
                return GV::B2;
            case AGV::X:
                return GV::X;
            case AGV::Y:
                return GV::Y;
            case AGV::OR:
                return GV::OR;
            case AGV::AS:
                return GV::AS;
            case AGV::SN:
                return GV::SN;
            case AGV::MN:
                return GV::MN;
            case AGV::US:
                return GV::US;
            case AGV::UM:
                return GV::UM;
            case AGV::GP:
                return GV::GP;
            case AGV::GE:
                return GV::GE;
            case AGV::SW:
                return GV::SW;
            case AGV::SH:
                return GV::SH;
            case AGV::PLA:
                return GV::PLA;
            case AGV::ZA:
                return GV::ZA;
            default:
                return GV::SW;
        }
    };

    const auto valid = pksm::VersionTables::availableItems(toCoreVersion(saveData->getVersion()));
    for (const auto id_i : valid) {
        const u16 id = static_cast<u16>(id_i);
        if (id <= 0) {
            continue;
        }

        if (existing.contains(id)) {
            continue;
        }

        auto name = i18n::item(pksm::Language::ENG, id);
        if (name.empty() || name == "None") {
            continue;
        }

        addItemIds.push_back(id);
        names.push_back(name);
    }
 
    if (names.empty()) {
        addItemList->SetDataSource({"No missing items"});
        addItemIds.clear();
    } else {
        addItemList->SetDataSource(names);
    }
}
 
void BagScreen::ConfirmAddSelectedItem() {
    if (!saveDataAccessor) {
        return;
    }
 
    auto saveData = saveDataAccessor->getCurrentSaveData();
    if (!saveData) {
        return;
    }
 
    if (!addItemList || addItemIds.empty()) {
        return;
    }
 
    const auto sel = addItemList->GetSelectedIndex();
    if ((sel < 0) || (static_cast<size_t>(sel) >= addItemIds.size())) {
        return;
    }
 
    pksm::saves::BagPouch pouch = pksm::saves::BagPouch::Unknown;
    if ((currentCategory >= 0) && (static_cast<size_t>(currentCategory) < categoryPouches.size())) {
        pouch = categoryPouches.at(static_cast<size_t>(currentCategory));
    }
    if (pouch == pksm::saves::BagPouch::Unknown) {
        return;
    }
 
    const auto id = addItemIds.at(static_cast<size_t>(sel));
 
    auto updated = saveData->getBagItems();
    updated.push_back(pksm::saves::BagItem{pouch, id, 1});
    saveData->setBagItems(std::move(updated));
    saveDataAccessor->markUnsavedChanges();
 
    HideAddItemOverlay();
    RefreshItemListForCurrentCategory();
    UpdateItemDisplay();
}
 
void BagScreen::ShowCategory(int categoryIndex) {
    currentCategory = categoryIndex;
    currentItemIndex = 0;
 
    if (categoryIndex >= 0 && categoryIndex < static_cast<int>(categoryLabels.size())) {
        for (auto& button : categoryButtons) {
            button->SetVisible(false);
        }
 
        categoryHeaderText->SetText(categoryLabels[categoryIndex]);
        categoryHeaderText->SetVisible(true);
 
        itemNameText->SetVisible(false);
 
        itemList->SetVisible(true);
        itemList->SetFocused(true);
 
        decreaseButton->SetVisible(true);
        increaseButton->SetVisible(true);
        itemQuantityText->SetVisible(true);
        backButton->SetVisible(true);
 
        RefreshItemListForCurrentCategory();
 
        UpdateItemDisplay();
 
        itemList->RequestFocus();
 
        std::vector<pksm::ui::HelpItem> helpItems = {
            {{{pksm::ui::global::ButtonGlyph::A}}, "Adjust Quantity"},
            {{{pksm::ui::global::ButtonGlyph::B}}, "Back to Main Menu"},
            {{{pksm::ui::global::ButtonGlyph::DPad}}, "Navigate Controls"},
            {{{pksm::ui::global::ButtonGlyph::X}}, "Add Item"},
        };
        helpFooter->SetHelpItems(helpItems);
    } else {
        categoryHeaderText->SetVisible(false);
        itemNameText->SetVisible(false);
        itemList->SetVisible(false);
        itemList->SetFocused(false);
        decreaseButton->SetVisible(false);
        increaseButton->SetVisible(false);
        itemQuantityText->SetVisible(false);
        backButton->SetVisible(false);
 
        for (auto& button : categoryButtons) {
            button->SetVisible(true);
        }
 
        RefreshCategories();
 
        std::vector<pksm::ui::HelpItem> helpItems = {
            {{{pksm::ui::global::ButtonGlyph::A}}, "Select Category"},
            {{{pksm::ui::global::ButtonGlyph::B}}, "Back to Main Menu"},
            {{{pksm::ui::global::ButtonGlyph::DPad}}, "Navigate Categories"},
        };
        helpFooter->SetHelpItems(helpItems);
    }
}
 
void BagScreen::RefreshItemListForCurrentCategory() {
    currentItemMap.clear();
 
    if (!itemList) {
        return;
    }

    if (!this->saveDataAccessor) {
        itemList->SetDataSource({"No save loaded"});
        return;
    }

    auto saveData = this->saveDataAccessor->getCurrentSaveData();
    if (!saveData) {
        itemList->SetDataSource({"No save loaded"});
        return;
    }

    pksm::saves::BagPouch pouch = pksm::saves::BagPouch::Unknown;
    if ((this->currentCategory >= 0) && (static_cast<size_t>(this->currentCategory) < categoryPouches.size())) {
        pouch = categoryPouches.at(static_cast<size_t>(this->currentCategory));
    }

    std::vector<std::string> names;
    const auto &items = saveData->getBagItems();
    for (size_t i = 0; i < items.size(); i++) {
        const auto &it = items[i];
        if (it.pouch != pouch) {
            continue;
        }

        auto name = i18n::item(pksm::Language::ENG, it.itemId);
        if (name.empty()) {
            name = "Item #" + std::to_string(it.itemId);
        }
        names.push_back(name);
        currentItemMap.push_back(i);
    }

    if (names.empty()) {
        itemList->SetDataSource({"No items"});
        currentItemMap.clear();
    } else {
        itemList->SetDataSource(names);
    }
}

void BagScreen::UpdateItemDisplay() {
    if (!saveDataAccessor) {
        return;
    }

    auto saveData = this->saveDataAccessor->getCurrentSaveData();
    if (!saveData) {
        itemNameText->SetText("No save loaded");
        itemQuantityText->SetText("x0");
        return;
    }

    const auto &items = saveData->getBagItems();

    if (!itemList || currentItemMap.empty()) {
        currentItemQuantity = 0;
        itemQuantityText->SetText("x0");
        return;
    }

    const auto sel = itemList->GetSelectedIndex();
    if ((sel < 0) || (static_cast<size_t>(sel) >= currentItemMap.size())) {
        currentItemQuantity = 0;
        itemQuantityText->SetText("x0");
        return;
    }

    const auto bag_idx = currentItemMap.at(static_cast<size_t>(sel));
    if (bag_idx >= items.size()) {
        currentItemQuantity = 0;
        itemQuantityText->SetText("x0");
        return;
    }

    const auto &cur = items.at(bag_idx);

    currentItemQuantity = cur.count;

    itemQuantityText->SetText("x" + std::to_string(currentItemQuantity));
}

} // namespace pksm::layout