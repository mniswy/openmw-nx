#include "switchpath.hpp"

#if defined(__SWITCH__)

#include <components/platform/switch.hpp>

namespace Files
{

    SwitchPath::SwitchPath(const std::string& /*applicationName*/) {}

    std::filesystem::path SwitchPath::getUserConfigPath() const
    {
        return Platform::Switch::getApplicationDirectory() / "config";
    }

    std::filesystem::path SwitchPath::getUserDataPath() const
    {
        return Platform::Switch::getApplicationDirectory() / "";
    }

    std::filesystem::path SwitchPath::getGlobalConfigPath() const
    {
        return {};
    }

    std::filesystem::path SwitchPath::getLocalPath() const
    {
        return Platform::Switch::getApplicationDirectory() / "";
    }

    std::filesystem::path SwitchPath::getGlobalDataPath() const
    {
        return Platform::Switch::getApplicationDirectory() / "";
    }

    std::filesystem::path SwitchPath::getCachePath() const
    {
        return Platform::Switch::getApplicationDirectory() / "cache";
    }

    std::vector<std::filesystem::path> SwitchPath::getInstallPaths() const
    {
        return {};
    }

} /* namespace Files */

#endif /* defined(__SWITCH__) */
