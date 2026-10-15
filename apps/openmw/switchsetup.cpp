#include "switchsetup.hpp"

#ifdef __SWITCH__

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <components/debug/debuglog.hpp>
#include <components/platform/switch.hpp>
#include <components/toutf8/toutf8.hpp>

#include "../mwiniimporter/importer.hpp"

namespace OMW
{
    void prepareSwitchConfiguration()
    {
        const std::filesystem::path& root = Platform::Switch::getApplicationDirectory();
        const std::filesystem::path configFile = root / "config" / "openmw.cfg";

        MwIniImporter::multistrmap cfg;
        if (std::filesystem::exists(configFile))
            cfg = MwIniImporter::loadCfgFile(configFile);

        // Already set up, or brought over from another OpenMW installation.
        if (cfg.contains("content"))
            return;

        const std::filesystem::path dataDir = root / "data";
        if (!std::filesystem::exists(dataDir / "Morrowind.esm"))
            throw std::runtime_error("Morrowind.esm was not found.\n\nCopy the contents of the \"Data Files\" "
                                     "folder of your Morrowind installation to sdmc:"
                + dataDir.string() + " and start OpenMW again.");

        std::filesystem::path iniFile;
        for (const std::filesystem::path& candidate : { root / "Morrowind.ini", dataDir / "Morrowind.ini" })
        {
            if (std::filesystem::exists(candidate))
            {
                iniFile = candidate;
                break;
            }
        }
        if (iniFile.empty())
            throw std::runtime_error("Morrowind.ini was not found.\n\nCopy Morrowind.ini from your Morrowind "
                                     "installation folder to sdmc:"
                + root.string() + " and start OpenMW again. It is only needed once.");

        Log(Debug::Info) << "First start: writing " << configFile << " from " << iniFile;

        if (!cfg.contains("encoding"))
            cfg["encoding"] = { "win1252" };

        MwIniImporter importer;
        importer.setInputEncoding(ToUTF8::calculateEncoding(cfg["encoding"].back()));

        MwIniImporter::multistrmap ini = importer.loadIniFile(iniFile);

        ini.erase("Fonts:Font 0");
        ini.erase("Fonts:Font 1");
        ini.erase("Fonts:Font 2");

        importer.merge(cfg, ini);
        importer.mergeFallback(cfg, ini);

        cfg.erase("fallback-archive");
        for (const std::string name : { "Morrowind", "Tribunal", "Bloodmoon" })
        {
            if (std::filesystem::exists(dataDir / (name + ".esm")))
                cfg["content"].push_back(name + ".esm");
            if (std::filesystem::exists(dataDir / (name + ".bsa")))
                cfg["fallback-archive"].push_back(name + ".bsa");
        }

        std::filesystem::create_directories(configFile.parent_path());
        std::ofstream stream(configFile);
        MwIniImporter::writeToFile(stream, cfg);
        if (!stream)
            throw std::runtime_error("Failed to write sdmc:" + configFile.string());
    }
}

#endif
