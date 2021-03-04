#pragma once

#ifndef LEVELCOMMAND_H
#define LEVELCOMMAND_H

#include "toonz/txshcolumn.h"

#include <set>
#include <QList>

class TXshLevel;

namespace LevelCmd {
void addMissingLevelsToCast(const QList<TXshColumnP>& columns);
void addMissingLevelsToCast(std::set<TXshLevel*>& levels);
}  // namespace LevelCmd

#endif
