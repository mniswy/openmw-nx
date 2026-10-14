#ifndef OPENMW_APPS_OPENMW_SWITCHSETUP_HPP
#define OPENMW_APPS_OPENMW_SWITCHSETUP_HPP

#ifdef __SWITCH__

namespace OMW
{
    /// Writes the user's openmw.cfg on first start.
    ///
    /// On other platforms the launcher and the installation wizard do this. Neither exists on the console,
    /// so the content list is derived from what was copied to the data directory and the game settings are
    /// imported from Morrowind.ini.
    ///
    /// \throws std::runtime_error with instructions for the user when the game data is not in place.
    void prepareSwitchConfiguration();
}

#endif

#endif
