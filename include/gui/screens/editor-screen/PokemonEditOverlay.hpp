#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <pu/ui/ui_Overlay.hpp>

#include "pksmcore/pkx/PKX.hpp"

namespace pksm::ui {

class PokemonEditOverlay : public pu::ui::Overlay {
public:
    enum class Result {
        None,
        Save,
        Cancel,
    };

    PokemonEditOverlay(pu::i32 x, pu::i32 y, pu::i32 width, pu::i32 height);
    PU_SMART_CTOR(PokemonEditOverlay)

    void SetPokemon(std::unique_ptr<pksm::PKX> pokemon);
    std::unique_ptr<pksm::PKX> TakePokemon();
    bool HasPokemon() const;

    void MoveUp();
    void MoveDown();
    void MoveLeft();
    void MoveRight();

    Result Confirm();
    Result Cancel();

private:
    enum class Tab : int {
        Stats = 0,
        Moves = 1,
        Misc = 2,
        Save = 3,
    };

    enum class FocusArea : int {
        Fields = 0,
        Tabs = 1,
    };

    struct FieldRow {
        std::string label;
        std::string value;
        bool editable;
    };

    void Rebuild();
    void ApplyDelta(int delta);
    std::vector<FieldRow> BuildRows() const;
    size_t CurrentRowCount() const;
    void ClampSelection();

    std::unique_ptr<pksm::PKX> pk;

    FocusArea focusArea = FocusArea::Tabs;
    Tab currentTab = Tab::Stats;
    int selectedTab = 0;
    std::array<int, 3> selectedRowsByTab = {0, 0, 0};
    int selectedRow = 0;
};

}  // namespace pksm::ui
