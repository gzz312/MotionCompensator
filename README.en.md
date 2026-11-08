# MotionCompensator

[中文](README.md) | **English**

A native Windows C++20 visual-assistance tool for game motion discomfort. It draws a transparent, click-through dot overlay and an optional central reticle, using Windows Graphics Capture, grayscale optical flow, and keyboard/mouse/controller input prediction. No hooks, DLL injection, game-memory reading, or anti-cheat bypasses.

**Current preview: 0.1.0-preview.1. Custom source-available and commercial integration license.** Dependencies retain their own licenses. Comfort effects vary by person, game and settings; general effectiveness has not been established.

## License

Free use, study, modification and sharing are permitted. Integration into a larger commercial work with substantial independent functionality is allowed with attribution to MotionCompensator and gzz312, an available source address, and preservation of the full license.

Standalone sale of the tool or modified/rebranded versions, paid standalone plugins, and separate paid unlocks of project-based functionality are prohibited. Trivial wrappers, minor changes and bundling do not qualify as commercial integration. Independent parts of a larger commercial work do not have to disclose their source solely because of integration.

The full Chinese terms in [LICENSE.txt](LICENSE.txt) govern; its English text is an informational summary. This is a custom source-available license, not MIT/GPL or an OSI-approved open-source license. Third-party licenses remain separate. No special standalone-sale exception is provided for the author.

## Getting started

The distributable is `MotionCompensator-0.1.0-preview.1-win-x64.zip`. Extract it and run `MotionCompensator.exe`. Published binaries will appear on the repository's [Releases page](https://github.com/gzz312/MotionCompensator/releases); if no release exists yet, build from source below. Local development packages live in `versions/0.1.0-preview.1/` and are excluded from the source repository.

The EXE embeds non-system dependencies, initial settings, instructions, credits and license texts. Installing OpenCV or a compiler is unnecessary for running a packaged build.

1. Close other versions, start your game and prefer borderless/windowed mode.
2. Leave automatic foreground-game detection enabled and switch to a recognized game. For an unknown game, refresh the window list, select its window and connect manually.
3. Select Regional dots, Screen-edge dots or Static dots. Each style retains separate visual settings. Dynamic styles never automatically switch to Static.
4. Switch back to the target game to view the overlay; it hides when you leave the game. Start with a short comparison and stop if discomfort develops.
5. For diagnostics, enable **Log**, reproduce the issue, then disable it. Files are saved beside the EXE in `logs/` and are not uploaded automatically.

Ctrl+Alt+F8 cycles styles; Ctrl+Alt+F9 shows/hides the overlay; Ctrl+Alt+F10 stops capture and automatic connection. The app's **中文 / English** button switches its interface language independently of these README pages.

## Features

- Continuously scrolling regional dots, fading screen-edge dots, and a fixed 4×4 static reference.
- Staggered layout, a clear center, dot size/transparency/color, and cross/diamond-cross reticles.
- Same-hue brightness adaptation and dual outlines; alternating edge-dot sizes, limited shrinking near fade-out and continuous direction reversal.
- A 9×9 grayscale analysis grid aggregated into nine motion regions, combined with input prediction and global/local observations.
- Debug vectors/curves, opt-in diagnostic logging, Chinese/English UI and light/dark/system themes.

## Limitations

Game comfort comparisons have primarily been performed by the user in PAYDAY 2 and other games. Software checks establish neither medical efficacy nor universal game compatibility. Exclusive fullscreen, motion blur, cuts, weapon/HUD occlusion and discrepancies between input intent and actual character motion can affect results. Multi-monitor/high-DPI coverage, full keyboard navigation and long-running stability need more testing. See [known issues](docs/en/KNOWN-ISSUES.md).

## Building from source

Requires Windows, a C++20 compiler, CMake, Ninja and matching static OpenCV 4.12 core/imgproc/video libraries. MSYS2 UCRT64 GCC 15.2 has been validated; the MSVC path has not.

```powershell
./build.ps1 -OpenCVRoot '<static OpenCV installation>'
./package.ps1 -OpenCVRoot '<static OpenCV installation>'
```

Tools may be on PATH or supplied with `-Compiler` and `-ToolsRoot`. Dependency caches, logs, previous binaries and build directories are excluded. See the [build guide](docs/en/BUILD.md).

## Documentation and credits

- [Validation record (Chinese)](docs/RELEASE-PREVIEW.md) · [Contributing and reporting issues](CONTRIBUTING.md#english)
- [Known issues](docs/en/KNOWN-ISSUES.md) · [Testing](docs/en/TESTING.md) · [Local data and privacy](docs/en/PRIVACY.md)
- [Design references and thanks](docs/en/CREDITS.md) · [Third-party licenses](docs/en/THIRD-PARTY.md)
- [Development record (Chinese)](docs/DEVELOPMENT.md) · [Version history (Chinese)](docs/HISTORY.md)

The screen-edge visual design was inspired by shsr07 (拾荒散人)'s [MotionStabilizer](https://github.com/shsr07/MotionStabilizer). Thanks to the author for sharing and discussing the design. [Author's Bilibili video](https://www.bilibili.com/video/BV1saKg6XEvg/). The scoped comparison of current sources found no direct code copying; this is not a guarantee of zero similarity. References do not imply participation or endorsement.

The repository contains source, build scripts and selected development records. Machine-local logs, dependency caches, old EXEs and temporary build files are excluded. The Releases page is authoritative for whether a binary release has actually been published.
