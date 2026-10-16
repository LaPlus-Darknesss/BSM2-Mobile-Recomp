# Save files and backups

Your progress is in the `SaveData` folder beside `BSM2Mobile.exe`. The launcher creates it when needed, so moving the complete game folder also keeps your save with it.

Close the game before editing or replacing these files.

To start over, delete both `Profile.save` and `OldProfile.save`. The second file is the game's fallback copy, so leaving it there can bring the old progress back. You can also delete `local_bsm2.settings` if you want to reset settings.

Before each launch, the runner copies the existing profile and settings into a dated folder under `SaveData/Backups`. Those backups are kept for recovery and are not loaded automatically. If you want to restore one, copy its profile files back into `SaveData` while the game is closed. Keep a copy of your current files first if you might want to return to them.

Deleting the whole `SaveData` folder also resets things, however, that removes the backup history as well. The launcher will recreate the folder the next time you play.

Cloud saving is not implemented. Keep a separate copy of this folder if you want an extra backup.
