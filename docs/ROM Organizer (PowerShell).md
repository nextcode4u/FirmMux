# ROM Organizer (PowerShell)

For large ROM collections, use:
https://github.com/nextcode4u/PowerShell-File-Organizer-Scripts

This external script pack helps organize files inside an existing ROM system folder after files are already placed there, for example:

- `sd:/roms/<system>/` (unorganized ROMs)

It is especially useful when a single system folder has thousands of files and you want quick A–Z buckets for browsing and cleanup.

## Scripts

- `Alphabetic organizer.ps1`
  - Sorts files in the current folder into `#` and `A`–`Z` subfolders.
- `Flatten back to root.ps1`
  - Moves files from subfolders back to the current root and removes empty folders.

## Requirements

- Windows
- PowerShell 5.1 or newer

If scripts are blocked, run:

```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

## Suggested workflow for big ROM sets

1. Download scripts from the repo above.
2. Put the scripts into a large unsorted ROM folder.
3. Run `Alphabetic organizer.ps1`.
4. Work one letter bucket at a time to clean duplicates, bad dumps, and naming issues.
5. Split cleaned files into system folders for FirmMux under `sd:/roms/<system>/`.
6. Use `Flatten back to root.ps1` if you want to undo bucket folders.

## How this helps with very large ROM collections

- Reduces overload:
  - Instead of one huge folder, you get smaller `#`, `A`–`Z` batches that are easier to review.
- Speeds manual QA:
  - You can verify names and remove duplicates in smaller chunks.
- Improves final structure:
  - After cleanup, move files to clear system paths like `sd:/roms/nds/`, `sd:/roms/gb/`, `sd:/roms/snes/`.
- Safe rollback:
  - If needed, flatten returns files back to one directory.

## Practical example

If you run `Alphabetic organizer.ps1` inside a folder, it organizes files in that same directory into:

- `#` for names starting with numbers/symbols
- `A` through `Z` for names starting with letters

It does not auto-detect systems or move files by ROM type.

Use it to organize files already inside `sd:/roms/<system>/` into filename buckets (`#`, `A`–`Z`) for easier browsing and cleanup.
