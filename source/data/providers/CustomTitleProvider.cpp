#include "data/providers/CustomTitleProvider.hpp"

#include "utils/Logger.hpp"

namespace {
std::vector<pksm::titles::Title::Ref> createInitialTitles() {
    std::vector<pksm::titles::Title::Ref> titles;
    
    // add custom titles (eventually)

    return titles;
}
}  // namespace

CustomTitleProvider::CustomTitleProvider() : customTitles(createInitialTitles()) {
    LOG_DEBUG("CustomTitleProvider initialized with " + std::to_string(customTitles.size()) + " titles");
}

std::vector<pksm::titles::Title::Ref> CustomTitleProvider::GetCustomTitles() const {
    return customTitles;
}