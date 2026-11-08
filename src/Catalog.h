#pragma once
#include <QList>
#include <QString>
#include <QStringList>

struct EmuDef {
    QString id, name, homepage;
    QStringList exeNames;
    QString githubRepo, assetRegex, assetRegexArm, directUrl;
    QStringList args;
};

struct EmuLink {
    QString emuId;
    QString core;
};

struct SystemDef {
    QString id, name, thumbDir;
    int igdbId;
    QStringList exts, hintExts, folderHints;
    QList<EmuLink> emus;
};

namespace Catalog {
const QList<EmuDef>& emulators();
const QList<SystemDef>& systems();
const EmuDef* emu(const QString& id);
const SystemDef* system(const QString& id);
}
