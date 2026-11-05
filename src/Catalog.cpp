#include "Catalog.h"

const QList<EmuDef>& Catalog::emulators() {
    static const QList<EmuDef> v = {
        {"dolphin", "Dolphin", "https://dolphin-emu.org/download/", {"Dolphin.exe", "dolphin-emu"}, "", "", "", "", {"-b", "-e", "{rom}"}},
        {"pcsx2", "PCSX2", "https://pcsx2.net/downloads/", {"pcsx2-qt.exe", "pcsx2-qt"}, "PCSX2/pcsx2", "windows-x64-Qt.*\\.7z$", "", "", {"-batch", "--", "{rom}"}},
        {"duckstation", "DuckStation", "https://www.duckstation.org/", {"duckstation-qt-x64-ReleaseLTCG.exe", "duckstation-qt.exe", "duckstation-qt"}, "stenzek/duckstation", "windows-x64-release\\.zip$", "windows-arm64-release\\.zip$", "", {"-batch", "--", "{rom}"}},
        {"ppsspp", "PPSSPP", "https://www.ppsspp.org/download", {"PPSSPPWindows64.exe", "PPSSPPSDL"}, "", "", "", "", {"{rom}"}},
        {"mgba", "mGBA", "https://mgba.io/downloads.html", {"mGBA.exe", "mgba-qt"}, "mgba-emu/mgba", "win64\\.7z$", "", "", {"{rom}"}},
        {"mesen", "Mesen 2", "https://www.mesen.ca/", {"Mesen.exe", "Mesen"}, "SourMesen/Mesen2", "Windows.*\\.zip$", "", "", {"{rom}"}},
        {"melonds", "melonDS", "https://melonds.kuribo64.net/downloads.php", {"melonDS.exe", "melonDS"}, "melonDS-emu/melonDS", "win.*x86_64.*\\.zip$", "", "", {"{rom}"}},
        {"azahar", "Azahar (3DS)", "https://azahar-emu.org/", {"azahar.exe", "azahar"}, "azahar-emu/azahar", "windows.*\\.zip$", "", "", {"{rom}"}},
        {"rpcs3", "RPCS3", "https://rpcs3.net/download", {"rpcs3.exe", "rpcs3"}, "RPCS3/rpcs3-binaries-win", "win64.*\\.7z$", "", "", {"{rom}"}},
        {"cemu", "Cemu", "https://cemu.info/", {"Cemu.exe", "cemu"}, "cemu-project/Cemu", "windows-x64\\.zip$", "", "", {"-g", "{rom}"}},
        {"project64", "Project64", "https://www.pj64-emu.com/downloads/", {"Project64.exe"}, "", "", "", "", {"{rom}"}},
        {"retroarch", "RetroArch", "https://www.retroarch.com/", {"retroarch.exe", "retroarch"}, "", "", "", "https://buildbot.libretro.com/nightly/windows/x86_64/RetroArch.7z", {"-L", "{core}", "{rom}"}},
    };
    return v;
}

const QList<SystemDef>& Catalog::systems() {
    static const QList<SystemDef> v = {
        {"nes", "Nintendo (NES)", "Nintendo - Nintendo Entertainment System", {"nes", "unf"}, {"nes", "famicom"}, {{"mesen", ""}, {"retroarch", "nestopia_libretro"}}},
        {"snes", "Super Nintendo", "Nintendo - Super Nintendo Entertainment System", {"sfc", "smc"}, {"snes", "sfc", "super nintendo"}, {{"mesen", ""}, {"retroarch", "snes9x_libretro"}}},
        {"n64", "Nintendo 64", "Nintendo - Nintendo 64", {"z64", "n64", "v64"}, {"n64", "nintendo 64"}, {{"project64", ""}, {"retroarch", "mupen64plus_next_libretro"}}},
        {"gc", "GameCube", "Nintendo - GameCube", {"gcm", "iso", "rvz", "gcz", "ciso"}, {"gc", "ngc", "gamecube", "gcn"}, {{"dolphin", ""}}},
        {"wii", "Wii", "Nintendo - Wii", {"wbfs", "wad", "iso", "rvz"}, {"wii"}, {{"dolphin", ""}}},
        {"wiiu", "Wii U", "Nintendo - Wii U", {"wua", "wud", "wux", "rpx"}, {"wiiu", "wii u"}, {{"cemu", ""}}},
        {"gb", "Game Boy", "Nintendo - Game Boy", {"gb"}, {"gb", "game boy"}, {{"mgba", ""}, {"mesen", ""}}},
        {"gbc", "Game Boy Color", "Nintendo - Game Boy Color", {"gbc"}, {"gbc"}, {{"mgba", ""}, {"mesen", ""}}},
        {"gba", "Game Boy Advance", "Nintendo - Game Boy Advance", {"gba"}, {"gba"}, {{"mgba", ""}, {"mesen", ""}}},
        {"nds", "Nintendo DS", "Nintendo - Nintendo DS", {"nds"}, {"nds", "ds"}, {{"melonds", ""}, {"retroarch", "desmume_libretro"}}},
        {"3ds", "Nintendo 3DS", "Nintendo - Nintendo 3DS", {"3ds", "cci", "cxi", "cia"}, {"3ds"}, {{"azahar", ""}}},
        {"ps1", "PlayStation", "Sony - PlayStation", {"cue", "chd", "pbp", "ecm"}, {"ps1", "psx", "psone", "playstation"}, {{"duckstation", ""}, {"retroarch", "pcsx_rearmed_libretro"}}},
        {"ps2", "PlayStation 2", "Sony - PlayStation 2", {"iso", "chd", "cso"}, {"ps2", "playstation 2", "playstation2"}, {{"pcsx2", ""}}},
        {"psp", "PSP", "Sony - PlayStation Portable", {"iso", "cso", "pbp"}, {"psp"}, {{"ppsspp", ""}}},
        {"ps3", "PlayStation 3", "Sony - PlayStation 3", {"sfb"}, {"ps3"}, {{"rpcs3", ""}}},
        {"genesis", "Mega Drive / Genesis", "Sega - Mega Drive - Genesis", {"md", "gen", "smd"}, {"genesis", "megadrive", "mega drive", "md"}, {{"retroarch", "genesis_plus_gx_libretro"}}},
    };
    return v;
}

const EmuDef* Catalog::emu(const QString& id) {
    for (const auto& e : emulators()) if (e.id == id) return &e;
    return nullptr;
}
const SystemDef* Catalog::system(const QString& id) {
    for (const auto& s : systems()) if (s.id == id) return &s;
    return nullptr;
}
