#pragma once

#include "Config.h"

namespace DiscordConfig {
inline constexpr const char* AppId = Config::DiscordAppId;

inline constexpr const char* LargeImageKey  = "emucenter";
inline constexpr const char* LargeImageText = "EmuCenter";
inline constexpr const char* SmallImagePrefix = "sys_";

inline constexpr const char* PlayDetails  = "Jogando %1";
inline constexpr const char* PlayState    = "no %1";
inline constexpr const char* PlayDetailsHidden = "Jogando";
inline constexpr const char* IdleDetails  = "Navegando na biblioteca";
inline constexpr const char* IdleState    = "%1 jogos";

inline constexpr const char* Button1Label = "";
inline constexpr const char* Button1Url   = "";
inline constexpr const char* Button2Label = "";
inline constexpr const char* Button2Url   = "";
}
