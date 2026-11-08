#pragma once
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#include "Secrets.example.h"
#endif

#ifndef EMUCENTER_DISCORD_APP_ID
#define EMUCENTER_DISCORD_APP_ID ""
#endif
#ifndef EMUCENTER_IGDB_CLIENT_ID
#define EMUCENTER_IGDB_CLIENT_ID ""
#endif
#ifndef EMUCENTER_IGDB_CLIENT_SECRET
#define EMUCENTER_IGDB_CLIENT_SECRET ""
#endif

namespace Config {
inline constexpr const char* AppVersion = "0.4.0";
inline constexpr const char* GithubRepo = "eugugaa/emucenter";
inline constexpr const char* UpdateAsset = "EmuCenter.zip";
inline constexpr const char* DiscordAppId = EMUCENTER_DISCORD_APP_ID;
inline constexpr const char* IgdbClientId = EMUCENTER_IGDB_CLIENT_ID;
inline constexpr const char* IgdbClientSecret = EMUCENTER_IGDB_CLIENT_SECRET;
}
