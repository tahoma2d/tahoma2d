
#if defined(WIN32)
#include "jconfig.vc"
#elif defined(LINUX) || defined(MACOSX)
#include "jconfig.gcc"
#else
#include "jconfig.irix"
#endif
