#pragma once

#include <memory>
#include <pu/pu_Include.hpp>
#include <pu/ui/render/render_Renderer.hpp>
#include <string>

namespace pksm::titles
{

    class Title
    {
    public:
        Title(const std::string &name, const std::string &iconPath, u64 titleId) : name(name), titleId(titleId)
        {
            const std::string fallbackIconPath = "romfs:/gfx/data/game_icons/lg_pikachu_icon.jpg";

            auto loadIcon = [](const std::string &path) -> pu::sdl2::TextureHandle::Ref
            {
                if (path.empty())
                {
                    return nullptr;
                }
                try
                {
                    auto image = pu::ui::render::LoadImage(path);
                    if (image)
                    {
                        return pu::sdl2::TextureHandle::New(image);
                    }
                }
                catch (...)
                {
                }
                return nullptr;
            };

            iconTexture = loadIcon(iconPath);
            if (!iconTexture)
            {
                iconTexture = loadIcon(fallbackIconPath);
            }
        }
        PU_SMART_CTOR(Title)

        // Getters
        const std::string &getName() const { return name; }
        u64 getTitleId() const { return titleId; }
        pu::sdl2::TextureHandle::Ref getIcon() const { return iconTexture; }

    private:
        std::string name;
        u64 titleId;
        pu::sdl2::TextureHandle::Ref iconTexture;
    };
} // namespace pksm::titles