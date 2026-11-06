#pragma once
// ============================================================================
//  Configuração do Discord Rich Presence — edite aqui e recompile.
//  Crie o app em https://discord.com/developers/applications e copie o
//  "Application ID". Em "Rich Presence > Art Assets" envie as imagens:
//    LargeImageKey            (ex.: "emucenter")
//    SmallImagePrefix + id    (ex.: "sys_n64", "sys_gc", "sys_ps2"...)  [opcional]
// ============================================================================

// Application ID EMBUTIDO NO .EXE. Deixe "" para não embutir: então o ID é
// digitado em Configurações > Avançado > Discord (fica só no seu PC).
// Também dá para definir sem editar este arquivo: cmake -DEMUCENTER_DISCORD_APP_ID=123...
#ifndef EMUCENTER_DISCORD_APP_ID
#define EMUCENTER_DISCORD_APP_ID ""
#endif

namespace DiscordConfig {
inline constexpr const char* AppId = EMUCENTER_DISCORD_APP_ID;

// imagens (chaves dos Art Assets enviados no portal do Discord)
inline constexpr const char* LargeImageKey  = "emucenter";
inline constexpr const char* LargeImageText = "EmuCenter";
inline constexpr const char* SmallImagePrefix = "sys_";   // + id do console. "" = sem ícone pequeno

// textos (%1 = veja ao lado)
inline constexpr const char* PlayDetails  = "Jogando %1";            // %1 = nome do jogo
inline constexpr const char* PlayState    = "no %1";                 // %1 = console
inline constexpr const char* PlayDetailsHidden = "Jogando";          // quando "mostrar nome do jogo" está desligado
inline constexpr const char* IdleDetails  = "Navegando na biblioteca";
inline constexpr const char* IdleState    = "%1 jogos";              // %1 = quantidade de jogos

// botões (até 2). Deixe label ou url vazios para não mostrar.
inline constexpr const char* Button1Label = "";
inline constexpr const char* Button1Url   = "";
inline constexpr const char* Button2Label = "";
inline constexpr const char* Button2Url   = "";
}
