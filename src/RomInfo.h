#pragma once
#include <QString>

namespace RomInfo {
// Reads the name stored INSIDE the ROM (header). Empty if the format doesn't have one or is unreadable.
QString internalName(const QString& path, const QString& systemId);
}