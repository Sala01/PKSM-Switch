#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <switch/types.h>

namespace pksm
{
    namespace data
    {
        namespace emulator
        {

            struct ExtraSaveSlot
            {
                std::string name;
                std::vector<std::string> paths;
            };

            struct EmulatorGameEntry
            {
                std::string name;
                std::string iconPath;
                u64 titleId = 0;
                std::vector<std::string> saveProbes;
                std::vector<ExtraSaveSlot> extraSaves;
            };

            class EmulatorGameCatalog
            {
            public:
                static std::vector<EmulatorGameEntry> LoadFromDataJson(const std::string &jsonPath);
                static std::optional<EmulatorGameEntry> FindByTitleId(const std::vector<EmulatorGameEntry> &entries, u64 titleId);

                static std::unordered_map<u64, EmulatorGameEntry> BuildIndexByTitleId(const std::vector<EmulatorGameEntry> &entries);
            };

        }

    }

} // namespace pksm