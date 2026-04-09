#pragma once

#include <vector>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include <switch/types.h>
#include <set>
#include "data/titles/Title.hpp" // use the existing Title class
#include "data/providers/interfaces/ITitleDataProvider.hpp"

class CustomTitleProvider;

namespace pksm::titles
{

    class TitleDataProvider : public ::ITitleDataProvider
    {
    public:
        using Ref = std::shared_ptr<TitleDataProvider>;

        TitleDataProvider(); // default constructor, loads JSON from fixed path

        std::vector<Title::Ref> GetInstalledTitles(const AccountUid &userId) const override;
        Title::Ref GetGameCardTitle() const override;
        unsigned long long GetInsertedGameCardID() const;
        std::vector<Title::Ref> GetEmulatorTitles() const override;
        std::vector<Title::Ref> GetCustomTitles() const override;
        void GetInstalledApplicationIds(std::set<u64> &out_ids) const;

    private:
        std::vector<std::shared_ptr<Title>> installedTitles;
        std::vector<Title::Ref> customTitles;

        std::shared_ptr<CustomTitleProvider> customTitleProvider;

        std::vector<Title::Ref> emulatorTitles;
    };

} // namespace pksm::titles