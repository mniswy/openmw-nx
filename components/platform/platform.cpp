#include "platform.hpp"

#ifdef WIN32
#include <stdio.h>
#endif

#ifdef __SWITCH__
#include "switch.hpp"
#endif

namespace Platform
{

    static void increaseFileHandleLimit()
    {
#ifdef WIN32
        // Increase limit for open files at the stream I/O level, see
        // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setmaxstdio?view=msvc-170#remarks
        _setmaxstdio(8192);
#else
        // No-op on any other platform.
#endif
    }

    void init()
    {
        increaseFileHandleLimit();
#ifdef __SWITCH__
        Switch::init();
#endif
    }

}
