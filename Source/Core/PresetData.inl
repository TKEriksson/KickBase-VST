// Fabrikspresets (inkluderas av Presets.cpp, i samma namnrymd som Preset och Value).
namespace
{
    const std::vector<Preset>& fullPresets()
    {
        static const std::vector<Preset> presets { { "Init", {} } };
        return presets;
    }

    const std::vector<Preset>& kickPresets()
    {
        static const std::vector<Preset> presets { { "Default", {} } };
        return presets;
    }

    const std::vector<Preset>& bassPresets()
    {
        static const std::vector<Preset> presets { { "Default", {} } };
        return presets;
    }

    const std::vector<Preset>& effectChainPresets()
    {
        static const std::vector<Preset> presets { { "Default (all off)", {} } };
        return presets;
    }
}
