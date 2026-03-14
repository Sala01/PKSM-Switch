#pragma once

#include <functional>
#include <pu/Plutonium>
#include <string>
#include <vector>

#include "gui/shared/UIConstants.hpp"

namespace pksm::ui {

class FontManager {
public:
    // Function type for font name generators (e.g., MakeHeavyFontName)
    using FontNameGenerator = std::function<std::string(u32)>;

    // All sizes that must be registered for every custom font face.
    // Keep this list in sync with every MakeHeavyFontName / MakeMediumFontName
    // call across the codebase so that RenderText never returns nullptr.
    static const std::vector<u32>& AllSizes() {
        static const std::vector<u32> sizes = {
            // Named constants (UIConstants.hpp)
            pksm::ui::global::FONT_SIZE_TITLE,               // 60
            pksm::ui::global::FONT_SIZE_HEADER,              // 40
            pksm::ui::global::FONT_SIZE_BUTTON,              // 40  (same value, deduped at runtime)
            pksm::ui::global::FONT_SIZE_BOX_BUTTON,          // 42
            pksm::ui::global::FONT_SIZE_TRIGGER_BUTTON_NAVIGATION, // 32
            pksm::ui::global::FONT_SIZE_BOX_SPACES_BUTTON,   // 32
            pksm::ui::global::FONT_SIZE_ACCOUNT_NAME,        // 28
            pksm::ui::global::FONT_SIZE_TRAINER_INFO_STATS,  // 36
            // Extra sizes used by EditorScreen info panel
            17u,
            18u,
            19u,
            22u,
            // Extra sizes used by EditorActionOverlay and MainMenu
            26u,
            35u,
            // Extra sizes used by PokemonEditOverlay
            24u,
            30u,
            34u,
        };
        return sizes;
    }

    // Register a font with all custom sizes
    static void RegisterFont(const std::string& fontPath, const FontNameGenerator& nameGenerator) {
        for (const auto size : AllSizes()) {
            auto font = std::make_shared<pu::ttf::Font>(size);
            font->LoadFromFile(fontPath);
            pu::ui::render::AddFont(nameGenerator(size), font);
        }
    }

    // Configure renderer options with all custom font sizes
    static void ConfigureRendererFontSizes(pu::ui::render::RendererInitOptions& renderer_opts) {
        for (const auto size : AllSizes()) {
            renderer_opts.AddExtraDefaultFontSize(size);
        }
    }
};

}  // namespace pksm::ui