<p align="center">
<img src="https://raw.githubusercontent.com/eugugaa/emucenter/refs/heads/main/logo_icon.png" alt="EmuCenter Logo" width="15%"/>
</p>

<div align="center">
<img src="https://img.shields.io/github/last-commit/eugugaa/emucenter" alt="GitHub last commit"/>
<img src="https://img.shields.io/github/commit-activity/w/eugugaa/emucenter" alt="GitHub commit activity"/>
<br>
<img src="https://img.shields.io/github/contributors/eugugaa/emucenter" alt="GitHub contributors"/>
<img src="https://img.shields.io/github/languages/code-size/eugugaa/emucenter" alt="Code size"/>
</div>

<h1 align="center">EmuCenter</h1>
<p align="center">A lightweight emulator hub. C++17 + Qt 6.x.</p>

## About

EmuCenter is a lightweight and user-friendly emulator hub designed to organize and manage your game collection in one place.

Instead of having to open different emulators and browse through folders to find your games, EmuCenter provides a centralized library where you can manage your ROMs, customize game information, and launch your favorite titles with ease.

The application automatically reads game names directly from supported ROM files, helping organize your collection without requiring manual configuration. With optional IGDB integration, you can also retrieve additional game information, including covers, banners, and descriptions.
## Features

    Centralized Game Library: Keep your games organized in one place, with an easy way to browse and launch them.
    Automatic Game Recognition: Reads game names directly from supported ROM formats.
    Game Customization: Customize individual game names, descriptions, icons, and banners.
    IGDB Integration: Retrieve additional metadata and artwork for your games.
    Emulator Management: Configure different emulators and associate them with your games.
    Custom Commands: Set individual launch commands for each game.
    Custom Themes: Personalize the application's appearance using custom themes.
    Startup Options: Configure Windows startup behavior, minimized startup, and startup delays.
    Single Instance: Prevents multiple instances of EmuCenter from running simultaneously.

EmuCenter is designed to make managing your emulation library simpler, more organized, and more convenient, while keeping you in control of your own game files and emulator configurations.

## What's New in 0.3
- Game names are read directly from ROM files (N64, GameCube, Wii, GB/GBC, GBA, DS, SNES, and Mega Drive).
- Optional IGDB integration to retrieve game names, covers, banners, and descriptions.
- Individual game settings, including name, description, icon, banner, emulator, and commands.
- Advanced settings: start with Windows, start minimized, startup delays, and simultaneous downloads.
- Support for custom themes in themes/all and themes/normal.
- Single-instance system, preventing multiple instances of the application from running simultaneously.

## Configuration
- Metadata: Configure your IGDB Client ID and Client Secret under Settings > Metadata.
- Emulators: Configure the paths to the emulators used by the application.
- Themes: Customize themes in the themes/all and themes/normal folders. See themes/LEIA-ME.txt.
- Advanced: Configure automatic startup, startup delays, and simultaneous downloads.

EmuCenter only uses your own ROMs.

Extraction of .zip and .7z archives uses the tar utility available in Windows 10/11.

## License

[GNU General Public License v2.0](https://github.com/eugugaa/emucenter/blob/main/LICENSE)

<p align="center">
made by gust!
</p>
