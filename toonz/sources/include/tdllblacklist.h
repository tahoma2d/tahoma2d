#pragma once

#ifndef T_DLL_BLACKLIST
#define T_DLL_BLACKLIST

#ifdef _WIN32
std::vector<QString> dllBlackList = {"lvcod64.dll", "ff_vfw.dll",
                                     "tsccvid64.dll", "hapcodec.dll"};

inline bool isDLLBlackListed(QString dllFile) {
  for (int x = 0; x < dllBlackList.size(); x++) {
    if (dllFile.contains(dllBlackList[x], Qt::CaseInsensitive)) {
      return true;
    }
  }

  return false;
}
#endif

#endif // T_DLL_BLACKLIST
