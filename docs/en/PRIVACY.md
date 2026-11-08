# Local data and privacy

[中文](../PRIVACY.md) | **English**

The application has no automatic upload, account login, cloud analysis or automatic updater. Capture and motion processing run locally.

- Preferences use the current user's `HKCU\Software\MotionCompensator`, with PreferencesV4 and older-format compatibility.
- Logging is off on every startup. Enabling **Log** writes to `logs/` beside the EXE; disabling it stops recording and keeps existing files.
- Diagnostics can include target process/window details, timestamps, frame rates, input-event statistics/control states, optical flow and compensation vectors. Arbitrary typed text is not logged. Logs are not anonymous: review before sharing.
- The normal logging switch does not save game screenshots. Explicit developer `--flow-debug` with diagnostic recording may save a limited number of grayscale frames; this is separate from normal logging.
- CSV rotation limits: 10 MiB per file, 100 MiB total, at most 20 files. Only regular `session-*.csv` files in that directory are managed; unrelated directories are not recursively cleaned.
- There is no game-memory reading, DLL injection or interception/modification of the game's input stream. External overlays and read-only input observation are not a guarantee that every game's rules permit use.

Prefer version details, reproduction steps and minimal reviewed log excerpts when reporting a problem. Do not upload an entire workspace containing unrelated personal data.
