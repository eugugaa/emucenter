#include "Catalog.h"

static EmuLink em(const char* id) { return {QString::fromLatin1(id), QString()}; }
static EmuLink ra(const char* core) { return {QStringLiteral("retroarch"), QString::fromLatin1(core) + "_libretro"}; }

const QList<EmuDef>& Catalog::emulators() {
    static const QList<EmuDef> v = {
        {"dolphin", "Dolphin", "https://dolphin-emu.org/download/", {"Dolphin.exe", "dolphin-emu"}, "", "", "", "", {"-b", "-e", "{rom}"}},
        {"pcsx2", "PCSX2", "https://pcsx2.net/downloads/", {"pcsx2-qt.exe", "pcsx2-qt"}, "PCSX2/pcsx2", "windows-x64-Qt\\.7z$", "", "", {"-batch", "--", "{rom}"}},
        {"duckstation", "DuckStation", "https://www.duckstation.org/", {"duckstation-qt-x64-ReleaseLTCG.exe", "duckstation-qt.exe", "duckstation-qt"}, "stenzek/duckstation", "windows-x64-release\\.zip$", "windows-arm64-release\\.zip$", "", {"-batch", "--", "{rom}"}},
        {"ppsspp", "PPSSPP", "https://www.ppsspp.org/download", {"PPSSPPWindows64.exe", "PPSSPPSDL"}, "", "", "", "", {"{rom}"}},
        {"mgba", "mGBA", "https://mgba.io/downloads.html", {"mGBA.exe", "mgba-qt"}, "mgba-emu/mgba", "win64\\.7z$", "", "", {"{rom}"}},
        {"mesen", "Mesen 2", "https://www.mesen.ca/", {"Mesen.exe", "Mesen"}, "SourMesen/Mesen2", "_Windows\\.zip$", "", "", {"{rom}"}},
        {"melonds", "melonDS", "https://melonds.kuribo64.net/downloads.php", {"melonDS.exe", "melonDS"}, "melonDS-emu/melonDS", "windows-x86_64\\.zip$", "windows-aarch64\\.zip$", "", {"{rom}"}},
        {"azahar", "Azahar (3DS)", "https://azahar-emu.org/", {"azahar.exe", "azahar"}, "azahar-emu/azahar", "^azahar-windows-msvc-[0-9.]+\\.zip$", "", "", {"{rom}"}},
        {"rpcs3", "RPCS3", "https://rpcs3.net/download", {"rpcs3.exe", "rpcs3"}, "RPCS3/rpcs3-binaries-win", "win64.*\\.7z$", "", "", {"{rom}"}},
        {"cemu", "Cemu", "https://cemu.info/", {"Cemu.exe", "cemu"}, "cemu-project/Cemu", "windows-x64\\.zip$", "", "", {"-g", "{rom}"}},
        {"project64", "Project64", "https://www.pj64-emu.com/downloads/", {"Project64.exe"}, "", "", "", "", {"{rom}"}},
        {"flycast", "Flycast", "https://github.com/flyinghead/flycast/releases", {"flycast.exe", "flycast"}, "flyinghead/flycast", "^flycast-win64-.*\\.zip$", "", "", {"{rom}"}},
        {"stella", "Stella", "https://stella-emu.github.io/downloads.html", {"Stella.exe", "stella"}, "stella-emu/stella", "windows\\.zip$", "", "", {"{rom}"}},
        {"xemu", "xemu (Xbox)", "https://xemu.app/", {"xemu.exe", "xemu"}, "xemu-project/xemu", "^xemu-[0-9.]+-windows-x86_64\\.zip$", "^xemu-[0-9.]+-windows-arm64\\.zip$", "", {"-dvd_path", "{rom}"}},
        {"xenia", "Xenia Canary (Xbox 360)", "https://github.com/xenia-canary/xenia-canary-releases/releases", {"xenia_canary.exe", "xenia.exe"}, "xenia-canary/xenia-canary-releases", "windows.*\\.zip$", "", "", {"{rom}"}},
        {"eden", "Eden (Switch)", "https://eden-emu.dev/downloads/", {"eden.exe", "Eden.exe"}, "", "", "", "", {"-g", "{rom}"}},
        {"retroarch", "RetroArch", "https://www.retroarch.com/", {"retroarch.exe", "retroarch"}, "", "", "", "https://buildbot.libretro.com/nightly/windows/x86_64/RetroArch.7z", {"-L", "{core}", "{rom}"}},
    };
    return v;
}

const QList<SystemDef>& Catalog::systems() {
    static const QList<SystemDef> v = {
        {"nes", "Nintendo (NES)", "Nintendo - Nintendo Entertainment System", 18, {"nes", "unf"}, {}, {"nes", "famicom"}, {em("mesen"), ra("nestopia")}},
        {"fds", "Famicom Disk System", "Nintendo - Family Computer Disk System", 51, {"fds"}, {}, {"fds"}, {em("mesen")}},
        {"snes", "Super Nintendo", "Nintendo - Super Nintendo Entertainment System", 19, {"sfc", "smc"}, {}, {"snes", "sfc", "super nintendo"}, {em("mesen"), ra("snes9x")}},
        {"n64", "Nintendo 64", "Nintendo - Nintendo 64", 4, {"z64", "n64", "v64"}, {}, {"n64", "nintendo 64"}, {em("project64"), ra("mupen64plus_next")}},
        {"gc", "GameCube", "Nintendo - GameCube", 21, {"gcm", "iso", "rvz", "gcz", "ciso"}, {}, {"gc", "ngc", "gamecube", "gcn"}, {em("dolphin")}},
        {"wii", "Wii", "Nintendo - Wii", 5, {"wbfs", "wad", "iso", "rvz"}, {}, {"wii"}, {em("dolphin")}},
        {"wiiu", "Wii U", "Nintendo - Wii U", 41, {"wua", "wud", "wux", "rpx"}, {}, {"wiiu", "wii u"}, {em("cemu")}},
        {"switch", "Nintendo Switch", "Nintendo - Nintendo Switch", 130, {"nsp", "xci"}, {}, {"switch", "nsw"}, {em("eden")}},
        {"switch2", "Nintendo Switch 2", "Nintendo - Nintendo Switch 2", 508, {}, {"nsp", "xci"}, {"switch 2", "switch2", "ns2", "nsw2"}, {}},
        {"gb", "Game Boy", "Nintendo - Game Boy", 33, {"gb"}, {}, {"gb", "game boy"}, {em("mgba"), em("mesen")}},
        {"gbc", "Game Boy Color", "Nintendo - Game Boy Color", 22, {"gbc"}, {}, {"gbc"}, {em("mgba"), em("mesen")}},
        {"gba", "Game Boy Advance", "Nintendo - Game Boy Advance", 24, {"gba"}, {}, {"gba"}, {em("mgba"), em("mesen")}},
        {"vb", "Virtual Boy", "Nintendo - Virtual Boy", 87, {"vb"}, {}, {"vb", "virtual boy"}, {ra("mednafen_vb")}},
        {"pokemini", "Pok\xc3\xa9mon Mini", "Nintendo - Pokemon Mini", 166, {"min"}, {}, {"pokemini", "pokemon mini"}, {ra("pokemini")}},
        {"nds", "Nintendo DS", "Nintendo - Nintendo DS", 20, {"nds"}, {}, {"nds", "ds"}, {em("melonds"), ra("desmume")}},
        {"3ds", "Nintendo 3DS", "Nintendo - Nintendo 3DS", 37, {"3ds", "cci", "cxi", "cia"}, {}, {"3ds"}, {em("azahar")}},

        {"ps1", "PlayStation", "Sony - PlayStation", 7, {"cue", "chd", "pbp", "ecm"}, {}, {"ps1", "psx", "psone", "playstation"}, {em("duckstation"), ra("pcsx_rearmed")}},
        {"ps2", "PlayStation 2", "Sony - PlayStation 2", 8, {"iso", "chd", "cso"}, {}, {"ps2", "playstation 2", "playstation2"}, {em("pcsx2")}},
        {"psp", "PSP", "Sony - PlayStation Portable", 38, {"iso", "cso", "pbp"}, {}, {"psp", "playstation portable"}, {em("ppsspp")}},
        {"ps3", "PlayStation 3", "Sony - PlayStation 3", 9, {"sfb"}, {}, {"ps3"}, {em("rpcs3")}},

        {"sms", "Master System", "Sega - Master System - Mark III", 64, {"sms"}, {}, {"sms", "master system", "mastersystem"}, {ra("genesis_plus_gx")}},
        {"gg", "Game Gear", "Sega - Game Gear", 35, {"gg"}, {}, {"gg", "game gear"}, {ra("genesis_plus_gx")}},
        {"sg1000", "SG-1000", "Sega - SG-1000", 84, {"sg"}, {}, {"sg1000", "sg-1000"}, {ra("genesis_plus_gx")}},
        {"genesis", "Mega Drive / Genesis", "Sega - Mega Drive - Genesis", 29, {"md", "gen", "smd"}, {"bin"}, {"genesis", "megadrive", "mega drive", "md"}, {ra("genesis_plus_gx")}},
        {"32x", "Sega 32X", "Sega - 32X", 30, {"32x"}, {}, {"32x"}, {ra("picodrive")}},
        {"segacd", "Sega CD / Mega-CD", "Sega - Mega-CD - Sega CD", 78, {}, {"cue", "chd", "iso"}, {"segacd", "sega cd", "mega cd", "megacd"}, {ra("genesis_plus_gx")}},
        {"saturn", "Sega Saturn", "Sega - Saturn", 32, {}, {"cue", "chd", "iso", "mdf"}, {"saturn"}, {ra("mednafen_saturn")}},
        {"dreamcast", "Dreamcast", "Sega - Dreamcast", 23, {"gdi", "cdi"}, {"chd", "cue"}, {"dreamcast", "dc"}, {em("flycast")}},

        {"atari2600", "Atari 2600", "Atari - 2600", 59, {"a26"}, {"bin"}, {"atari 2600", "atari2600", "2600"}, {em("stella"), ra("stella")}},
        {"atari5200", "Atari 5200", "Atari - 5200", 66, {"a52"}, {"bin"}, {"atari 5200", "atari5200", "5200"}, {ra("a5200")}},
        {"atari7800", "Atari 7800", "Atari - 7800", 60, {"a78"}, {"bin"}, {"atari 7800", "atari7800", "7800"}, {ra("prosystem")}},
        {"lynx", "Atari Lynx", "Atari - Lynx", 61, {"lnx"}, {}, {"lynx"}, {ra("handy")}},
        {"jaguar", "Atari Jaguar", "Atari - Jaguar", 62, {"j64", "jag"}, {}, {"jaguar"}, {ra("virtualjaguar")}},
        {"atarist", "Atari ST", "Atari - ST", 63, {"st", "stx", "msa"}, {}, {"atari st", "atarist"}, {ra("hatari")}},

        {"pce", "PC Engine / TurboGrafx-16", "NEC - PC Engine - TurboGrafx 16", 86, {"pce"}, {"cue", "chd"}, {"pce", "pc engine", "turbografx", "tg16"}, {ra("mednafen_pce_fast")}},
        {"ngp", "Neo Geo Pocket", "SNK - Neo Geo Pocket", 119, {"ngp", "ngc"}, {}, {"ngp", "neo geo pocket"}, {ra("mednafen_ngp")}},
        {"ws", "WonderSwan", "Bandai - WonderSwan", 57, {"ws", "wsc"}, {}, {"wonderswan", "ws", "wsc"}, {ra("mednafen_wswan")}},
        {"coleco", "ColecoVision", "Coleco - ColecoVision", 68, {"col"}, {}, {"colecovision", "coleco"}, {ra("gearcoleco")}},
        {"intv", "Intellivision", "Mattel - Intellivision", 67, {"int"}, {"bin"}, {"intellivision", "intv"}, {ra("freeintv")}},
        {"c64", "Commodore 64", "Commodore - 64", 15, {"d64", "t64", "prg", "crt"}, {}, {"c64", "commodore 64"}, {ra("vice_x64")}},
        {"amiga", "Commodore Amiga", "Commodore - Amiga", 16, {"adf"}, {}, {"amiga"}, {ra("puae")}},
        {"msx", "MSX", "Microsoft - MSX", 27, {"mx1", "mx2"}, {"rom", "dsk"}, {"msx"}, {ra("bluemsx")}},
        {"3do", "3DO", "The 3DO Company - 3DO", 50, {}, {"iso", "cue", "chd"}, {"3do"}, {ra("opera")}},
        {"xbox", "Xbox", "Microsoft - Xbox", 11, {}, {"iso"}, {"xbox"}, {em("xemu")}},
        {"x360", "Xbox 360", "Microsoft - Xbox 360", 12, {"xex"}, {"iso"}, {"x360", "xbox 360", "xbox360"}, {em("xenia")}},
        {"neogeo", "Neo Geo", "SNK - Neo Geo", 80, {"neo"}, {"zip"}, {"neogeo", "neo geo"}, {ra("fbneo")}},
        {"arcade", "Arcade", "FBNeo - Arcade Games", 52, {}, {"zip", "7z"}, {"arcade", "mame", "fbneo", "fba"}, {ra("fbneo")}},
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
