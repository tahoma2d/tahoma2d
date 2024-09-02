#pragma once

#ifndef DDLBLACKLIST_H
#define DDLBLACKLIST_H

#ifdef _WIN32

// Blacklist entries must be all lowercase
std::vector<QString> dllBlackList = {"lvcod64.dll", "ff_vfw.dll"};

bool isDLLBlacklisted(QString dll) {
  // Added blacklisted DLLS known to cause crashes
  bool blacklisted = false;
  for (int x = 0; x < dllBlackList.size(); x++) {
    blacklisted = dll.toLower().contains(dllBlackList[x]);
    if (blacklisted) break;
  }

  return blacklisted;
}

#endif

#endif
