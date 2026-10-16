# BSM2 Mobile Recomp

I put this together to get the Android version of Bloons Supermonkey 2 running on Windows, with mouse controls similar to BSM2 Flash. It uses the original mobile 1.10 engine, translated ahead of time and compiled for Windows x64. The game runs locally without an Android emulator.

Keep in mind that this is still an early release candidate. The menu, map, lab, first-level gameplay and saving have been checked, however, I have not finished a full playthrough or tested every weapon and power. If you run into something that looks wrong, please include what you were using and where it happened.

## Getting it running

You need your own Android 1.10 game files. This repository includes the runtime adapters, translation tools and build instructions, but does not include the APK, game assets, original engine, generated game code or a prebuilt game executable. The importer checks the exact arm64 engine version before generating anything.

See [Building on Windows](docs/building.md) for the full setup. Once you have built the local game folder, double-click `Play.cmd` inside it. Keep that folder together.

## Controls

Move your mouse over the game to move Super Monkey during levels. You do not need to hold the mouse button down. Left-click still works as a tap for buttons and menus, and the game keeps its normal automatic firing.

The window has a fixed size for now, since resizing it could cause the graphics to break. Close the window or press Escape to quit normally. The launcher backs up your existing save before starting the game.

## Saves

Progress is stored in `SaveData`, beside the game executable. Close the game before changing anything there. To start from scratch, delete both `Profile.save` and `OldProfile.save`; you can also remove `local_bsm2.settings` to reset settings. Dated backups are kept in `SaveData/Backups` and are not loaded automatically.

There are more details in [Save files and backups](docs/saves.md).

## What has been changed

The Windows runtime adapts the Android interfaces used by the original engine. This includes graphics, input, audio, files, threads, numeric parsing and text handling. The compatibility checks also caught a random-number difference that was collapsing the spread of powers such as Glue Striker and Mortar toward the center of the screen.

I am keeping these fixes in the shared interfaces where possible, so the original game can keep using its own weapon data, collisions and progression. This project is separate from my custom Flash stages and balance changes.

See [Compatibility and current limits](docs/compatibility.md) for what has been checked and what is still open. Cloud accounts, ads, purchases and other online services are not implemented. Local saving is the supported way to keep progress.

## Credits

BSM2 and its original game files belong to Ninja Kiwi. This is an unofficial fan preservation project, and is not affiliated with Ninja Kiwi.

The translation/runtime foundation is [androidrecomp](https://github.com/sp00nznet/androidrecomp). Other dependencies and their licenses are listed in [PROVENANCE.md](PROVENANCE.md). The license in this repository applies to the project source, with the included third-party files retaining their own terms. It does not grant rights to the original game files.
