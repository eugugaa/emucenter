#pragma once
#include <QList>
#include <QString>
#include <QStringList>

// Definição de um emulador (como achar, baixar e executar)
struct EmuDef {
    QString id, name, homepage;
    QStringList exeNames;   // nomes possíveis do executável
    QString githubRepo;     // "dono/repo" -> baixa último release
    QString assetRegex;     // asset do release para Windows x64
    QString assetRegexArm;  // asset para Windows ARM64 (opcional)
    QString directUrl;      // URL fixa (alternativa ao GitHub)
    QStringList args;       // {rom} e {core} são substituídos
};

// Emulador + (opcional) core do RetroArch usado para um console
struct EmuLink { QString emuId; QString core; };

struct SystemDef {
    QString id, name, thumbDir;   // thumbDir = pasta no thumbnails.libretro.com
    QStringList exts, folderHints;
    QList<EmuLink> emus;          // 1º = recomendado, demais = alternativas
};

namespace Catalog {
const QList<EmuDef>& emulators();
const QList<SystemDef>& systems();
const EmuDef* emu(const QString& id);
const SystemDef* system(const QString& id);
}
