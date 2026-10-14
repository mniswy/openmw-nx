#ifndef COMPONENTS_FILES_SWITCHPATH_H
#define COMPONENTS_FILES_SWITCHPATH_H

#if defined(__SWITCH__)

#include <filesystem>
#include <string>
#include <vector>

/**
 * \namespace Files
 */
namespace Files
{

    /**
     * \struct SwitchPath
     *
     * Everything lives in the directory the application was installed to on the SD card
     * (normally /switch/openmw), so that it can be set up and backed up as a single folder.
     */
    struct SwitchPath
    {
        explicit SwitchPath(const std::string& applicationName);

        /**
         * \brief Return path to the user directory: <install dir>/config
         */
        std::filesystem::path getUserConfigPath() const;

        /**
         * \brief Return path to the directory with saves and screenshots: <install dir>
         */
        std::filesystem::path getUserDataPath() const;

        /**
         * \brief There is no system-wide configuration.
         */
        std::filesystem::path getGlobalConfigPath() const;

        /**
         * \brief Return path to the directory the application was installed to.
         */
        std::filesystem::path getLocalPath() const;

        std::filesystem::path getGlobalDataPath() const;

        std::filesystem::path getCachePath() const;

        std::vector<std::filesystem::path> getInstallPaths() const;
    };

} /* namespace Files */

#endif /* defined(__SWITCH__) */

#endif /* COMPONENTS_FILES_SWITCHPATH_H */
