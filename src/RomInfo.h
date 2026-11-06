#pragma once
#include <QString>

namespace RomInfo {
// Lê o nome gravado DENTRO da ROM (cabeçalho). Vazio se o formato não tem/for ilegível.
QString internalName(const QString& path, const QString& systemId);
}
